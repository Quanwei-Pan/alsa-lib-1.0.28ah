/**
 *@name:			pcm_play.c
 *@author: ss.pan
 *@time:   201707121150
 *@description:
 *  a demo of alsa audio play
 *@usage:
 * 1. need alsa-lib  and libasound.so supported
 * 2. cross_compile
 *   export LD_LIBRARY_PATH=$PWD:$LD_LIBRARY_PATH
 *   arm-linux-gcc -o pcm_record pcm_record.c -L -lasound
 **/

	#include <stdio.h>
	#include <stdlib.h>
	#include <malloc.h>
	#include <string.h>
	#include <sched.h>
	#include <errno.h>
	#include <getopt.h>
	#include <alsa/asoundlib.h>

static char *device = "default";    /* playback device */
static snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;  /* sample format */
static unsigned int rate = 44100;    /* stream rate */
static unsigned int channels = 1;    /* count of channels */
static unsigned int buffer_time = 200000;   /* ring buffer length in us */
static unsigned int period_time = 100000;   /* period time in us */
static int resample = 1;     /* enable alsa-lib resampling */
static int period_event = 0;     /* produce poll event after each period */
static int err;

static snd_pcm_sframes_t buffer_size;
static snd_pcm_sframes_t period_size;

/*
 *   Set pcm hardware ware parameters
 */
static int set_hwparams(snd_pcm_t *handle,
																								snd_pcm_hw_params_t *params,
																								snd_pcm_access_t access)
{
								unsigned int rrate;
								snd_pcm_uframes_t size;
								int err, dir = 0;

								/* choose all parameters */
								err = snd_pcm_hw_params_any(handle, params);
								if (err < 0) {
																printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
																return err;
								}
								/* set hardware resampling */
								err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
								if (err < 0) {
																printf("Resampling setup failed for playback: %s\n", snd_strerror(err));
																return err;
								}
								/* set the interleaved read/write format */
								err = snd_pcm_hw_params_set_access(handle, params, access);
								if (err < 0) {
																printf("Access type not available for playback: %s\n", snd_strerror(err));
																return err;
								}
								/* set the sample format */
								err = snd_pcm_hw_params_set_format(handle, params, format);
								if (err < 0) {
																printf("Sample format not available for playback: %s\n", snd_strerror(err));
																return err;
								}
								/* set the count of channels */
								err = snd_pcm_hw_params_set_channels(handle, params, channels);
								if (err < 0) {
																printf("Channels count (%i) not available for playbacks: %s\n", channels, snd_strerror(err));
																return err;
								}
								/* set the stream rate */
								rrate = rate;
								err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, &dir);
								//err = snd_pcm_hw_params_set_rate(handle, params, rrate, 0);
								if (err < 0) {
																printf("Rate %iHz not available for playback: %s\n", rate, snd_strerror(err));
																return err;
								}
								if (rrate != rate) {
																printf("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
																return -EINVAL;
								}
								/* set the buffer time */
								err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, &dir);
								//err = snd_pcm_hw_params_set_buffer_time(handle, params, buffer_time, dir);
								if (err < 0) {
																printf("Unable to set buffer time %i for playback: %s\n", buffer_time, snd_strerror(err));
																return err;
								}
								err = snd_pcm_hw_params_get_buffer_size(params, &size);
								if (err < 0) {
																printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
																return err;
								}
								buffer_size = size;
								/* set the period time */
								err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, &dir);
								//err = snd_pcm_hw_params_set_period_time(handle, params, period_time, dir);
								if (err < 0) {
																printf("Unable to set period time %i for playback: %s\n", period_time, snd_strerror(err));
																return err;
								}
								err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
								if (err < 0) {
																printf("Unable to get period size for playback: %s\n", snd_strerror(err));
																return err;
								}
								period_size = size;
								/* write the parameters to device */
								err = snd_pcm_hw_params(handle, params);
								if (err < 0) {
																printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
																return err;
								}
								return 0;
}

/*
 *   Set pcm soft ware parameters
 */
static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
{
								int err;

								/* get the current swparams */
								err = snd_pcm_sw_params_current(handle, swparams);
								if (err < 0) {
																printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
																return err;
								}
								/* start the transfer when the buffer is almost full: */
								/* (buffer_size / avail_min) * avail_min */
								err = snd_pcm_sw_params_set_start_threshold(handle, swparams, (buffer_size / period_size) * period_size);
								if (err < 0) {
																printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
																return err;
								}
								/* allow the transfer when at least period_size samples can be processed */
								/* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
								err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_event ? buffer_size : period_size);
								if (err < 0) {
																printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
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
								/* write the parameters to the playback device */
								err = snd_pcm_sw_params(handle, swparams);
								if (err < 0) {
																printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
																return err;
								}
								return 0;
}

/*
 *   Underrun and suspend recovery
 */
static int xrun_recovery(snd_pcm_t *handle, int err)
{
								if (err == -EPIPE) { /* under-run */
																err = snd_pcm_prepare(handle);
																if (err < 0)
																								printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
																return 0;
								} else if (err == -ESTRPIPE) {
																while ((err = snd_pcm_resume(handle)) == -EAGAIN)
																								sleep(1); /* wait until the suspend flag is released */
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
 * write audio file into pcm hardware
 */
static int write_loop(snd_pcm_t *play_handle, FILE *fp)
{
								int write_size = (period_size * channels * snd_pcm_format_physical_width(format)) / 8;
								char *samples = (char *) malloc(write_size);
								memset(samples, 0, write_size);
								if (samples == NULL) {
																printf("No enough memory\n");
																exit(EXIT_FAILURE);
								}
								printf("Write buffer size is %d bytes\n", write_size);

								while (1)
								{
																err = fread(samples, 1, write_size, fp);
																if(err == 0)
																{
																								printf("End of playing\n");
																								break;
																}
																err = snd_pcm_writei(play_handle, samples, period_size);
																if (err == -EAGAIN)
																								continue;
																if (err < 0) {
																								if (xrun_recovery(play_handle, err) < 0) {
																																printf("Write error: %s\n", snd_strerror(err));
																																exit(EXIT_FAILURE);
																								}
																								break;             // skip one period
																}

								}
								free(samples);
								samples = NULL;
								return 0;
}

int main(int argc, char *argv[])
{
								snd_pcm_t *handle;
								snd_pcm_hw_params_t *hwparams;
								snd_pcm_sw_params_t *swparams;

								printf("%s was compiled on %s at %s\n", __FILE__, __DATE__, __TIME__);

								if (argc != 2) {
																printf("Error format, it should be: alsa_pcm_play [music_name]\n");
																exit(1);
								}
								FILE *fp = fopen(argv[1], "rb");
								if(fp == NULL)
								{
																perror ("Error! ");
																return 0;
								}
								fseek(fp,  ftell(fp), SEEK_SET);//set the file stream with a new position indicator

								snd_pcm_hw_params_alloca(&hwparams);
								snd_pcm_sw_params_alloca(&swparams);

								printf("Playback device is %s\n", device);
								printf("Stream parameters are %iHz, %s, %i channels\n", rate, snd_pcm_format_name(format), channels);

								if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
																printf("Playback open error: %s\n", snd_strerror(err));
																return 0;
								}

								if ((err = set_hwparams(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
																printf("Setting of hwparams failed: %s\n", snd_strerror(err));
																exit(EXIT_FAILURE);
								}
								if ((err = set_swparams(handle, swparams)) < 0) {
																printf("Setting of swparams failed: %s\n", snd_strerror(err));
																exit(EXIT_FAILURE);
								}

								printf("Playing audio...\n");
								write_loop(handle, fp);
								fclose(fp);
								snd_pcm_close(handle);
								return 0;
}
