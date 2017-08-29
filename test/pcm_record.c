/**
   *@name:  		pcm_record.c
   *@author: 		ss.pan
   *@create_time:   	201707141155
   *@last_mod_time: 	201708221118
   *@description:
   *  a demo of alsa audio record to examine the functions
   *@usage:
   * 1. need alsa-lib  and libasound.so supported
   * 2. cross_compile
   *   ${CROSS_COMPILE}gcc example.c -o example -I ./include -L
   *   ./libs/libasound.so.2 ./libs/libasound.so.2.0.0 -Wl,-rpath-link ./libs
 **/

#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>
#include <string.h>

//include file here
#include "alsa/asoundlib.h"

static char *device = "default";    /* capture device */
static snd_pcm_format_t format = SND_PCM_FORMAT_S32_LE;  /* sample format */
static unsigned int rate = 48000;    /* stream rate */
static unsigned int channels = 8;    /* count of channels */
static unsigned int buffer_time = 20000;   /* buffer length in us */
static unsigned int period_time = 400000;   /* period time in us */
static unsigned int record_time = 24 * 60 * 60;   /* record time in s */
static unsigned int real_buff_time;
static int resample = 1;     /* enable alsa-lib resampling */
static int period_event = 0;     /* produce poll event after each period */
static int err;
FILE *fp;

static snd_pcm_sframes_t buffer_size;
static snd_pcm_sframes_t period_size;

/*
   *@brief   Set pcm hardware ware parameters
 */
static int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_access_t access)
{
	unsigned int rrate;
	snd_pcm_uframes_t size;
	int err;

	/* choose all parameters */
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		printf("Broken configuration for capture: no configurations available: %s\n", snd_strerror(err));
		return err;
	}
	/* set hardware resampling */
	err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
	if (err < 0) {
		printf("Resampling setup failed for capture: %s\n", snd_strerror(err));
		return err;
	}
	/* set the interleaved read/write format */
	err = snd_pcm_hw_params_set_access(handle, params, access);
	if (err < 0) {
		printf("Access type not available for capture: %s\n", snd_strerror(err));
		return err;
	}
	/* set the sample format */
	err = snd_pcm_hw_params_set_format(handle, params, format);
	if (err < 0) {
		printf("Sample format not available for capture: %s\n", snd_strerror(err));
		return err;
	}
	/* set the count of channels */
	err = snd_pcm_hw_params_set_channels(handle, params, channels);
	if (err < 0) {
		printf("Channels count (%i) not available for captures: %s\n", channels, snd_strerror(err));
		return err;
	}
	/* set the stream rate */
	rrate = rate;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
	if (err < 0) {
		printf("Rate %iHz not available for capture: %s\n", rate, snd_strerror(err));
		return err;
	}
	if (rrate != rate) {
		printf("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
		return -EINVAL;
	}
	/* set the buffer time */
	err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, 0);
	if (err < 0) {
		printf("Unable to set buffer time %i for capture: %s\n", buffer_time, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_get_buffer_size(params, &size);
	if (err < 0) {
		printf("Unable to get buffer size for capture: %s\n", snd_strerror(err));
		return err;
	}
	buffer_size = size;
	/* set the period time */
	err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, 0);
	if (err < 0) {
		printf("Unable to set period time %i for capture: %s\n", period_time, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_get_period_size(params, &size, 0);
	if (err < 0) {
		printf("Unable to get period size for capture: %s\n", snd_strerror(err));
		return err;
	}
	period_size = size;
	/* write the parameters to device */
	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		printf("Unable to set hw params for capture: %s\n", snd_strerror(err));
		return err;
	}
	return 0;
}

/*
   *@brief   Set pcm soft ware parameters
 */
static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
{
	int err;

	/* get the current swparams */
	err = snd_pcm_sw_params_current(handle, swparams);
	if (err < 0) {
		printf("Unable to determine current swparams for capture: %s\n", snd_strerror(err));
		return err;
	}
	/* start the transfer when the buffer is almost full: */
	/* (buffer_size / avail_min) * avail_min */
	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, (buffer_size / period_size) * period_size);
	if (err < 0) {
		printf("Unable to set start threshold mode for capture: %s\n", snd_strerror(err));
		return err;
	}
	/* allow the transfer when at least period_size samples can be processed */
	/* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
	err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_event ? buffer_size : period_size);
	if (err < 0) {
		printf("Unable to set avail min for capture: %s\n", snd_strerror(err));
		return err;
	}
	/* enable period events when requested */
	if (period_event) {
		err = snd_pcm_sw_params_set_period_event(handle, swparams, 1);
		if (err < 0) {
			printf("Unable to set period event: %s\n", snd_strerror(err));
			return err;
		}
	}
	/* write the parameters to the capture device */
	err = snd_pcm_sw_params(handle, swparams);
	if (err < 0) {
		printf("Unable to set sw params for capture: %s\n", snd_strerror(err));
		return err;
	}
	return 0;
}

/*
   *@brief   Underrun and suspend recovery
 */
static int xrun_recovery(snd_pcm_t *handle, int err)
{
	if (err == -EPIPE) {         /* under-run */
		err = snd_pcm_prepare(handle);
		if (err < 0)
			printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
			return 0;
	} else if (err == -ESTRPIPE) {
		while ((err = snd_pcm_resume(handle)) == -EAGAIN)
			sleep(1);                         /* wait until the suspend flag is released */
			if (err < 0) {
				err = snd_pcm_prepare(handle);
				if (err < 0)
				printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
			}
			return 0;
	}
	return err;
}

/*
   *@brief  read audio stream from pcm hardware
 */
static int read_loop(snd_pcm_t *play_handle, FILE *fp)
{
	unsigned int i;
	int read_size = (buffer_size * channels * snd_pcm_format_physical_width(format)) / 8;
	char *samples = (char *) malloc(read_size);
	memset(samples, 0, read_size);
	if (samples == NULL) {
		printf("No enough memory\n");
		exit(EXIT_FAILURE);
	}
	record_time = record_time * 1000000 / real_buff_time;
	i = record_time / 8;
	while (record_time > 0)
	{
		record_time--;

	//test code begin
		if(record_time == i * 7)
		{
			snd_dummy_generate_file(5);
		}
		if(record_time == i * 5)
		{
			err = snd_dummy_set_trigger(SND_DUMMY_TRIGGER_DISABLE);
			if(err == -1)
			{
				printf("Cannot turn off dummy read trigger\n");
				return -1;
			}
			err = snd_dummy_generate_file(7);
			if(err == -1)
			{
				printf("Get a 7 sec file failed!\n");
			}

		}
		if(record_time == i * 4)
		{
			err = snd_dummy_set_trigger(SND_DUMMY_TRIGGER_ENABLE);
			if(err == -1)
			{
				printf("Cannot set dummy read trigger\n");
				return -1;
			}
		}
		if(record_time == i * 3)
		{
			err = snd_dummy_generate_file(9);
			if(err == -1)
			{
				printf("Get a 9 sec file failed!\n");
			}
		}

	 //test code end

		err = snd_pcm_readi(play_handle, samples, buffer_size);
		if (err == -EAGAIN)
			continue;
			if (err < 0) {
				if (xrun_recovery(play_handle, err) < 0) {
					printf("Read error: %s\n", snd_strerror(err));
					exit(EXIT_FAILURE);
				}
			break;                // skip one period
			}
			err = fwrite(samples, sizeof(char), read_size, fp);
			//printf("the current file pointer is at %ld\n", ftell(fp));
	}
	printf("End of record\n");
	free(samples);
	samples = NULL;
	return 0;
}

int main(int argc, char *argv[])
{
	snd_pcm_t *capture_handle;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_sw_params_t *swparams;
	char *filename;
	char *filename1 = "/tmp/dummy_read.pcm";  //filename for audio hack

	printf("%s was compiled on %s at %s\n", __FILE__, __DATE__, __TIME__);

	err = snd_dummy_init(filename1, 20 * 224000 ); //max audio length sets with 20 sec
	if(err == -1)
	{
		printf("Error for snd audio hack init\n");
		return -1;
	}

	err = snd_dummy_set_trigger(SND_DUMMY_TRIGGER_ENABLE);
	if(err == -1)
	{
		printf("Cannot set dummy read trigger\n");
		return -1;
	}

	//creat a new  record_file
	if (argc != 2) {
		printf("Error format, it should be: ./pcm_record [file_name]\n");
		exit(1);
	}
	filename = argv[1];
	remove(filename);  //delete the same name file
	//created record file
	fp = fopen(filename, "wb");
	if(fp == NULL)
	{
		fprintf(stderr, "Error create: [%s]\n", filename);
		return -1;
	}

	snd_pcm_hw_params_alloca(&hwparams);
	snd_pcm_sw_params_alloca(&swparams);

	printf("Capture device is %s\n", device);
	printf("Stream parameters are %iHz, %s, %i channels\n", rate, snd_pcm_format_name(format), channels);

	if ((err = snd_pcm_open(&capture_handle, device, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		printf("Capture open error: %s\n", snd_strerror(err));
		return 0;
	}
	if ((err = set_hwparams(capture_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		printf("Setting of hwparams failed: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	if ((err = set_swparams(capture_handle, swparams)) < 0) {
		printf("Setting of swparams failed: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	printf("Start recording audio in %d sec ...\n", record_time);
	snd_pcm_hw_params_get_buffer_time(hwparams, &real_buff_time, 0);
	read_loop(capture_handle, fp);
	fclose(fp);
	snd_pcm_close(capture_handle);
	return 0;
}
