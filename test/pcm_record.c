/**
   *@name:			pcm_record.c
   *@author: ss.pan
   *@time:   201707141155
   *@description:
 *  a demo of alsa audio record
 **@usage:
 * 1. need alsa-lib  and libasound.so supported
 * 2. cross_compile
 *   export LD_LIBRARY_PATH=$PWD:$LD_LIBRARY_PATH
 *   arm-linux-gcc -o pcm_record pcm_record.c -lasound
 **/

#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <alsa/asoundlib.h>
#include <time.h>
#include <string.h>

static char *device = "hw:0,0";    /* capture device */
static snd_pcm_format_t format = SND_PCM_FORMAT_S16;  /* sample format */
static unsigned int rate = 44100;    /* stream rate */
static unsigned int channels = 1;    /* count of channels */
static unsigned int buffer_time = 2000;   /* ring buffer length in us */
static unsigned int period_time = 1000;   /* period time in us */
static unsigned int record_time = 72;   /* record time in s */
static unsigned int real_buff_time;
static int resample = 1;     /* enable alsa-lib resampling */
static int period_event = 0;     /* produce poll event after each period */
static int err;
FILE *fp;

static snd_pcm_sframes_t buffer_size;
static snd_pcm_sframes_t period_size;

extern int snd_dummy_trigger(int index);

/*
   *@brief   Set pcm hardware ware parameters
 */
static int set_hwparams(snd_pcm_t *handle,
																								snd_pcm_hw_params_t *params,
																								snd_pcm_access_t access)
{
								unsigned int rrate;
								snd_pcm_uframes_t size;
								int err, dir;

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
								err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, &dir);
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
								err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, &dir);
								if (err < 0) {
																printf("Unable to set period time %i for capture: %s\n", period_time, snd_strerror(err));
																return err;
								}
								err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
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
								int read_size = (buffer_size * channels * snd_pcm_format_physical_width(format)) / 8;
								char *samples = (char *) malloc(read_size);
								memset(samples, 0, read_size);
								if (samples == NULL) {
																printf("No enough memory\n");
																exit(EXIT_FAILURE);
								}
								record_time = record_time * 1000000 / real_buff_time;

								while (record_time > 0)
								{
																record_time--;
                                // printf("record_time  is %d\n", record_time);
																// if(record_time == 2000 * 7)
																// 								snd_dummy_trigger(0);
																// if(record_time == 2000 * 6)
																// 								snd_dummy_trigger(1);
																// if(record_time == 2000 * 5)
																// 								snd_dummy_trigger(0);
																// if(record_time == 2000 * 4)
																// 								snd_dummy_trigger(1);
																// if(record_time == 2000 * 3)
																// 								snd_dummy_trigger(1);
																// if(record_time == 2000 * 2)
																// 								snd_dummy_trigger(0);
																// if(record_time == 2000)
																// 								snd_dummy_trigger(0);
																err = snd_pcm_readi(play_handle, samples, buffer_size);
																if (err == -EAGAIN)
																								continue;
																if (err < 0) {
																								if (xrun_recovery(play_handle, err) < 0) {
																																printf("Read error: %s\n", snd_strerror(err));
																																exit(EXIT_FAILURE);
																								}
																								break;                                     // skip one period
																}
																err = fwrite(samples, sizeof(char), read_size, fp);
																printf("the current file pointer is at %ld\n", ftell(fp));
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

								snd_dummy_trigger(1); //open alsa dummy file
								snd_dummy_init(72 * 1024, 3 * 1024 * 1024, 36 * 1024);
								printf("%s was compiled on %s at %s\n", __FILE__, __DATE__, __TIME__);

								//creat a new  record_file
								if (argc != 2) {
																printf("Error format, it should be: ./pcm_record -f [file_name]\n");
																exit(1);
								}
								filename = argv[1];
								remove(filename);  //delete the same name file
								//created record file
								fp = fopen(filename, "wb+");
								if(fp == NULL)
								{
																fprintf(stderr, "Error create: [%s]\n", filename);
																return -1;
								}
								fseek(fp, ftell(fp), SEEK_SET); //set the file stream with a new position indicator

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