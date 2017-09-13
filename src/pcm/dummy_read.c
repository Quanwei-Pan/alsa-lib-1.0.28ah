#ifdef __cplusplus
extern "C" {
#endif
/*==================================================================================================

	Module Name: dummy_read.c

	General Description: This file implements the main functions of the dummy_read module.

====================================================================================================

                               Xiaomi Confidential Proprietary
                        (c) Copyright Xiaomi 2017, All Rights Reserved


Revision History:
                            Modification
Author                          Date        Description of Changes
-------------------------   ------------    -------------------------------------------
Quanwei Pan                  08/17/2017     Initial version
Quanwei Pan                  09/05/2017     Add opus encoding
====================================================================================================
                                        INCLUDE FILES
==================================================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <syslog.h>
#include <opus_codec.h>
#include "opus/opus.h"
#include "dummy_read.h"
#include "resample.h"

/*==================================================================================================
										CONSTANTS
==================================================================================================*/
#define DUMMY_READ_INPUT_SAMPLERATE				(48000)
#define DUMMY_READ_INPUT_CHANNLENUM				(8)
#define DUMMY_READ_INPUT_BYTEWIDTH				(4)
#define DUMMY_READ_INPUT_SIZE_INBYTE_PERSECOND	(DUMMY_READ_INPUT_SAMPLERATE * \
												DUMMY_READ_INPUT_CHANNLENUM * \
												DUMMY_READ_INPUT_BYTEWIDTH)

#define DUMMY_READ_MAX_PROCESS_TIME				(5)
#define DUMMY_READ_OUTPUT_SAMPLERATE			(16000)
#define DUMMY_READ_OUTPUT_CHANNLENUM			(7)
#define DUMMY_READ_OUTPUT_BYTEWIDTH				(2)
#define DUMMY_READ_OUTPUT_SIZE_INBYTE_PERSECOND	(DUMMY_READ_OUTPUT_SAMPLERATE * \
												DUMMY_READ_OUTPUT_CHANNLENUM * \
												DUMMY_READ_OUTPUT_BYTEWIDTH)
#define DUMMY_PROCESS_FRAME_COUNT				(300)
#define DUMMY_READ_PROCESS_SIZE_PERCYCLE		(DUMMY_PROCESS_FRAME_COUNT * \
												DUMMY_READ_INPUT_CHANNLENUM)

#define DUMMY_MAX_ALSA_FRAME_COUNT				(1024)
#define DUMMY_READ_PROCESS_ASSERT
/*==================================================================================================
						Static variables / structure and other definations
==================================================================================================*/
typedef struct
{
	short *pBase;
	int front;
	int rear;
	unsigned int maxsize;
}QUEUE, *PQUEUE;

typedef struct
{
	int *pBase;
	int front;
	int rear;
	unsigned int maxsize;
}StageQUEUE, *PStageQUEUE;

typedef struct
{
	bool dummy_flag;
	int dummy_file_size_per_channel;
	int dummy_max_alsa_frame_count;
	int dummy_queue_size_inbyte;
	int *dummy_stage_buffer;
	short *dummy_reformat_buffer;
	int *dummy_resampler_ram_buffer;
	short *dummy_output_buffer;
	PQUEUE dummy_queue[DUMMY_READ_OUTPUT_CHANNLENUM];
	PStageQUEUE dummy_stage_queue;
	pthread_mutex_t dummy_read_mutex;
	pthread_mutex_t dummy_read_mutex_queue;
	char *dummy_encoder_stage_buffer;
	char *dummy_encoder_output_buffer;
	char dummy_file_name[255];
	WebRtcSpl_State48khzTo16khz *dummy_resampler_handler[DUMMY_READ_OUTPUT_CHANNLENUM];
}Dummy_Read_Handler_t;

static Dummy_Read_Handler_t dummy_read_handler = {
	.dummy_flag = false,
	.dummy_file_size_per_channel = 0,
	.dummy_max_alsa_frame_count = 0,
	.dummy_queue_size_inbyte = 0,
	.dummy_reformat_buffer = 0,
	.dummy_resampler_ram_buffer = NULL,
	.dummy_stage_buffer = NULL,
	.dummy_output_buffer = NULL,
	.dummy_read_mutex = PTHREAD_MUTEX_INITIALIZER,
	.dummy_read_mutex_queue = PTHREAD_MUTEX_INITIALIZER,
	.dummy_file_name = "/tmp/mibrain/dummy_read.opus"
};

static WebRtcSpl_State48khzTo16khz resampler[DUMMY_READ_OUTPUT_CHANNLENUM];
static QUEUE Dummy_Read_Queue[DUMMY_READ_OUTPUT_CHANNLENUM];
static StageQUEUE Dummy_Read_stage_Queue;
static index_num = 0;
#ifdef DUMMY_FILE_BEFORE_RESAMPLE
	static FILE *fp1;
#endif
/*==================================================================================================
                                      STATIC FUNCTIONS
==================================================================================================*/
static Dummy_Read_ReturnValue_t CreateQueue(PQUEUE Q, unsigned int maxsize)
{
	Q->pBase = (short *) malloc(sizeof(short) * maxsize);
	if (NULL == Q->pBase)
	{
		syslog(LOG_ERR, "%s: Memory allocate failed for %d bytes \n", __func__, maxsize);
		return DUMMY_READ_RETURNVALUE_ERROR;
	}
	memset(Q->pBase, 0, maxsize * sizeof(short));
	Q->front = 0;
	Q->rear = 0;
	Q->maxsize = maxsize;
	return DUMMY_READ_RETURNVALUE_OK;
}
static Dummy_Read_ReturnValue_t CreateStageQueue(PStageQUEUE Q, unsigned int maxsize)
{
	Q->pBase = (int *) malloc(sizeof(int) * maxsize);
	if (NULL == Q->pBase)
	{
		syslog(LOG_ERR, "%s: Memory allocate failed for %d bytes \n", __func__, maxsize);
		return DUMMY_READ_RETURNVALUE_ERROR;
	}
	memset(Q->pBase, 0, maxsize * sizeof(int));
	Q->front = 0;
	Q->rear = 0;
	Q->maxsize = maxsize;
	return DUMMY_READ_RETURNVALUE_OK;
}
static Dummy_Read_ReturnValue_t FreeQueue(QUEUE *Q)
{
	if (Q->pBase != NULL)
	{
		free(Q->pBase);
		Q->pBase = NULL;
	}
	return DUMMY_READ_RETURNVALUE_OK;
}
static Dummy_Read_ReturnValue_t FreeStageQueue(PStageQUEUE Q)
{
	if (Q->pBase != NULL)
	{
		free(Q->pBase);
		Q->pBase = NULL;
	}
	return DUMMY_READ_RETURNVALUE_OK;
}
static Dummy_Read_ReturnValue_t EnQueue(PQUEUE Q, short val)
{
	Q->pBase[Q->rear] = val;
	Q->rear = (Q->rear + 1) % Q->maxsize;
	return DUMMY_READ_RETURNVALUE_OK;
}
static Dummy_Read_ReturnValue_t EnStageQueue(PStageQUEUE Q, int val)
{
	Q->pBase[Q->rear] = val;
	Q->rear = (Q->rear + 1) % Q->maxsize;
	return  DUMMY_READ_RETURNVALUE_OK;
}
static Dummy_Read_ReturnValue_t DeQueue(PQUEUE Q, short *val)
{
	*val = Q->pBase[Q->front];
	Q->front = (Q->front + 1) % Q->maxsize;
	return DUMMY_READ_RETURNVALUE_OK;
}
static Dummy_Read_ReturnValue_t DeStageQueue(PStageQUEUE Q, int *val)
{
	*val = Q->pBase[Q->front];
	Q->front = (Q->front + 1) % Q->maxsize;
	return DUMMY_READ_RETURNVALUE_OK;
}
static int QueryStageQueue(PStageQUEUE Q)
{
	return (Q->rear - Q->front + Q->maxsize) % Q->maxsize;
}

/*==================================================================================================
                                      GLOBAL FUNCTIONS
==================================================================================================*/
Dummy_Read_ReturnValue_t Dummy_Read_Init(char *file_name, int mem_size_inbyte)
{
	dummy_read_handler.dummy_queue_size_inbyte = mem_size_inbyte / 7;
	dummy_read_handler.dummy_max_alsa_frame_count = DUMMY_MAX_ALSA_FRAME_COUNT;
	strcpy(dummy_read_handler.dummy_file_name, file_name);
	/* Create queues for opus */
	int i;
	for(i = 0; i < DUMMY_READ_OUTPUT_CHANNLENUM; i++)
	{
		dummy_read_handler.dummy_queue[i] = &Dummy_Read_Queue[i];
		CreateQueue(dummy_read_handler.dummy_queue[i], dummy_read_handler.dummy_queue_size_inbyte >> 1);
	}
	/* Create Stage Queue */
	dummy_read_handler.dummy_stage_queue = &Dummy_Read_stage_Queue;
	CreateStageQueue(dummy_read_handler.dummy_stage_queue,(DUMMY_MAX_ALSA_FRAME_COUNT + \
		DUMMY_PROCESS_FRAME_COUNT) * DUMMY_READ_INPUT_CHANNLENUM);
	/* Allocate opus encoder output buffer */
	dummy_read_handler.dummy_encoder_output_buffer = (char *) malloc(DUMMY_READ_OUTPUT_SAMPLERATE * \
		DUMMY_READ_OUTPUT_BYTEWIDTH * DUMMY_READ_MAX_PROCESS_TIME >> 1);
	if (dummy_read_handler.dummy_encoder_output_buffer == NULL)
	{
		syslog(LOG_ERR, "%s: Fail to allocate opus encoder output buffer in %d bytes\n", __func__, \
			DUMMY_READ_OUTPUT_SAMPLERATE * DUMMY_READ_OUTPUT_BYTEWIDTH * DUMMY_READ_MAX_PROCESS_TIME >> 1);
		return DUMMY_READ_RETURNVALUE_ERROR;
	}
	else
	{
		printf("%s: Allocate opus encoder output buffer in %d bytes\n", __func__, \
			DUMMY_READ_OUTPUT_SAMPLERATE * DUMMY_READ_OUTPUT_BYTEWIDTH * DUMMY_READ_MAX_PROCESS_TIME >> 1);
	}
	/* Allocate opus encoder stage buffer */
	dummy_read_handler.dummy_encoder_stage_buffer = (char *) malloc(DUMMY_READ_OUTPUT_SAMPLERATE * \
		DUMMY_READ_OUTPUT_CHANNLENUM * DUMMY_READ_MAX_PROCESS_TIME);
	if (dummy_read_handler.dummy_encoder_stage_buffer == NULL)
	{
		syslog(LOG_ERR, "%s: Fail to allocate opus encoder stage buffer in %d bytes\n", __func__, \
			DUMMY_READ_OUTPUT_SAMPLERATE *  DUMMY_READ_OUTPUT_CHANNLENUM * DUMMY_READ_MAX_PROCESS_TIME);
		return DUMMY_READ_RETURNVALUE_ERROR;
	}
	else
	{
		printf("%s: Allocate opus encoder stage buffer in %d bytes\n", __func__, \
		DUMMY_READ_OUTPUT_SAMPLERATE * DUMMY_READ_OUTPUT_BYTEWIDTH * DUMMY_READ_MAX_PROCESS_TIME);
	}
	/* Allocate stage buffer */
	dummy_read_handler.dummy_stage_buffer = (int *) malloc(DUMMY_PROCESS_FRAME_COUNT * \
		DUMMY_READ_INPUT_BYTEWIDTH * DUMMY_READ_INPUT_CHANNLENUM);
	if (dummy_read_handler.dummy_stage_buffer == NULL)
	{
		syslog(LOG_ERR, "%s: Fail to allocate stage buffer in %d bytes\n", __func__, \
			DUMMY_PROCESS_FRAME_COUNT * DUMMY_READ_INPUT_BYTEWIDTH * DUMMY_READ_INPUT_CHANNLENUM);
		return DUMMY_READ_RETURNVALUE_ERROR;
	}
	else
	{
		printf("%s: Allocate %d bytes for stage buffer\n", __func__,\
			DUMMY_PROCESS_FRAME_COUNT * DUMMY_READ_INPUT_BYTEWIDTH* DUMMY_READ_INPUT_CHANNLENUM);
	}
	/* Allocate reformat buffer */
	dummy_read_handler.dummy_reformat_buffer = (short *) malloc(DUMMY_PROCESS_FRAME_COUNT * \
		DUMMY_READ_OUTPUT_CHANNLENUM * DUMMY_READ_OUTPUT_BYTEWIDTH);
	if (dummy_read_handler.dummy_reformat_buffer == NULL)
	{
		syslog(LOG_ERR, "%s: Fail to allocate resampler buffer in %d bytes\n", __func__, \
			DUMMY_PROCESS_FRAME_COUNT * DUMMY_READ_OUTPUT_CHANNLENUM * DUMMY_READ_OUTPUT_BYTEWIDTH);
		return DUMMY_READ_RETURNVALUE_ERROR;
	}
	else
	{
		printf("%s: Allocate %d bytes for resampler buffer\n", __func__,\
			DUMMY_PROCESS_FRAME_COUNT * DUMMY_READ_OUTPUT_CHANNLENUM * DUMMY_READ_OUTPUT_BYTEWIDTH);
	}
	/* Allocate resample output buffer */
	dummy_read_handler.dummy_output_buffer = (short *) malloc(DUMMY_PROCESS_FRAME_COUNT / 3 * \
		DUMMY_READ_OUTPUT_BYTEWIDTH);
	if (dummy_read_handler.dummy_output_buffer == NULL)
	{
		syslog(LOG_ERR, "%s: Fail to allocate resample output buffer in %d bytes\n", __func__, \
			DUMMY_PROCESS_FRAME_COUNT / 3 * DUMMY_READ_OUTPUT_BYTEWIDTH);
		return DUMMY_READ_RETURNVALUE_ERROR;
	}
	else
	{
		printf("%s: Allocate %d bytes for resample output buffer\n", __func__,\
			DUMMY_PROCESS_FRAME_COUNT / 3 * DUMMY_READ_OUTPUT_BYTEWIDTH);
	}
	/* Allocate resampler ram buffer */
	dummy_read_handler.dummy_resampler_ram_buffer = (int *)malloc(((DUMMY_PROCESS_FRAME_COUNT * \
		sizeof(int)) << 1) + 32 * sizeof(int));
	if (dummy_read_handler.dummy_resampler_ram_buffer == NULL)
	{
		syslog(LOG_ERR, "%s: Fail to allocate resampler ram buffer in %ld bytes\n", __func__, \
			((DUMMY_PROCESS_FRAME_COUNT * sizeof(int)) << 1) + 32 * sizeof(int));
		return DUMMY_READ_RETURNVALUE_ERROR;
	}
	else
	{
		printf("%s: Allocate %ld bytes for resampler ram buffer\n", __func__, \
			((DUMMY_PROCESS_FRAME_COUNT * sizeof(int)) << 1) + 32 * sizeof(int));
	}
	/* Create thread mutex lock*/
	pthread_mutex_init(&dummy_read_handler.dummy_read_mutex,NULL);
	pthread_mutex_init(&dummy_read_handler.dummy_read_mutex_queue,NULL);
	/* Init resampler for each channel */
	for(i = 0; i < DUMMY_READ_OUTPUT_CHANNLENUM; i++)
	{
		dummy_read_handler.dummy_resampler_handler[i] = &resampler[i];
		WebRtcSpl_ResetResample48khzTo16khz(dummy_read_handler.dummy_resampler_handler[i]);
	}

	return DUMMY_READ_RETURNVALUE_OK;
}

Dummy_Read_ReturnValue_t Dummy_Read_Finalize(void)
{
	/* free all allocated memory */
	int i;
	for(i = 0; i < DUMMY_READ_OUTPUT_CHANNLENUM; i++)
	{
		if(dummy_read_handler.dummy_queue[i] != NULL)
		{
			FreeQueue(dummy_read_handler.dummy_queue[i]);
			dummy_read_handler.dummy_queue[i] = NULL;
		}
	}
	if (dummy_read_handler.dummy_stage_queue != NULL)
	{
		FreeStageQueue(dummy_read_handler.dummy_stage_queue);
		dummy_read_handler.dummy_stage_queue = NULL;
	}
	if (dummy_read_handler.dummy_reformat_buffer != NULL)
	{
		free(dummy_read_handler.dummy_reformat_buffer);
		dummy_read_handler.dummy_reformat_buffer = NULL;
	}
	if (dummy_read_handler.dummy_resampler_ram_buffer != NULL)
	{
		free(dummy_read_handler.dummy_resampler_ram_buffer);
		dummy_read_handler.dummy_resampler_ram_buffer = NULL;
	}
	if (dummy_read_handler.dummy_output_buffer != NULL)
	{
		free(dummy_read_handler.dummy_output_buffer);
		dummy_read_handler.dummy_output_buffer = NULL;
	}
	if (dummy_read_handler.dummy_stage_buffer != NULL)
	{
		free(dummy_read_handler.dummy_stage_buffer);
		dummy_read_handler.dummy_stage_buffer = NULL;
	}
	if (dummy_read_handler.dummy_encoder_output_buffer != NULL)
	{
		free(dummy_read_handler.dummy_encoder_output_buffer);
		dummy_read_handler.dummy_encoder_output_buffer = NULL;
	}
	if (dummy_read_handler.dummy_encoder_output_buffer != NULL)
	{
		free(dummy_read_handler.dummy_encoder_output_buffer);
		dummy_read_handler.dummy_encoder_output_buffer = NULL;
	}
	pthread_mutex_destroy(&dummy_read_handler.dummy_read_mutex_queue);
	pthread_mutex_destroy(&dummy_read_handler.dummy_read_mutex);
	/* Reset flag */
	dummy_read_handler.dummy_flag = false;

	return DUMMY_READ_RETURNVALUE_OK;
}

Dummy_Read_ReturnValue_t Dummy_Read_Set_Trigger(bool enable)
{
	pthread_mutex_lock(&dummy_read_handler.dummy_read_mutex);
	dummy_read_handler.dummy_flag = enable;
	pthread_mutex_unlock(&dummy_read_handler.dummy_read_mutex);
	syslog(LOG_ERR, "%s: Dummy Trigger has been %s\n", __func__, enable ? "on" : "off");
	return DUMMY_READ_RETURNVALUE_OK;
}

Dummy_Read_ReturnValue_t Dummy_Read_Generate_File(int time_in_sec)
{
	bool dummy_flag;
	pthread_mutex_lock(&dummy_read_handler.dummy_read_mutex);
	dummy_flag = dummy_read_handler.dummy_flag;
	pthread_mutex_unlock(&dummy_read_handler.dummy_read_mutex);
	if (dummy_flag == false)
	{
		syslog(LOG_ERR, "%s: Audio hack has been closed, cannot create audio file \n", __func__);
		return DUMMY_READ_RETURNVALUE_ERROR;
	}
	if (time_in_sec <= 0 || time_in_sec > 5 )
	{
		syslog(LOG_ERR, "%s: Invalid time input %d sec ,it should be 1 ~ 5 sec\n", __func__, time_in_sec);
		return DUMMY_READ_RETURNVALUE_ERROR;
	}
	/* Get write file size */
	dummy_read_handler.dummy_file_size_per_channel = time_in_sec * DUMMY_READ_OUTPUT_SAMPLERATE;
	sprintf(dummy_read_handler.dummy_file_name,"/tmp/mico/xiaomi/xiaomi_wuw%d.opus",index_num++);
	FILE *fp = fopen(dummy_read_handler.dummy_file_name,"wb");
	if (fp == NULL)
	{
		syslog(LOG_ERR, "%s: File %s open failed\n", __func__, dummy_read_handler.dummy_file_name);
		return DUMMY_READ_RETURNVALUE_ERROR;
	}
	/* Opus encoder */
	int i,j;
	int outputsize;
	short *tmp = (short *) dummy_read_handler.dummy_encoder_stage_buffer;
	pthread_mutex_lock(&dummy_read_handler.dummy_read_mutex_queue);
	for(i = 0; i < DUMMY_READ_OUTPUT_CHANNLENUM; i++)
	{
		dummy_read_handler.dummy_queue[i]->front = (dummy_read_handler.dummy_queue[i]->rear + \
			dummy_read_handler.dummy_queue[i]->maxsize - dummy_read_handler.dummy_file_size_per_channel) % \
			dummy_read_handler.dummy_queue[i]->maxsize;
	}
	pthread_mutex_unlock(&dummy_read_handler.dummy_read_mutex_queue);
	for(i = 0; i < DUMMY_READ_OUTPUT_CHANNLENUM; i++)
	{
		for(j = 0; j < dummy_read_handler.dummy_file_size_per_channel; j++)
		{
			DeQueue(dummy_read_handler.dummy_queue[i], tmp + j);
		}
	#ifdef OPUS_ENCODE_ASSERT
		mi_opus((char *)tmp, dummy_read_handler.dummy_file_size_per_channel * sizeof(short), \
			dummy_read_handler.dummy_encoder_output_buffer, &outputsize, 1);
	#endif
		fwrite(dummy_read_handler.dummy_encoder_output_buffer, outputsize, 1, fp);
	}
	fclose(fp);
	printf("%s: %d second audio file %s has generated!\n", __func__, time_in_sec, dummy_read_handler.dummy_file_name);

	return DUMMY_READ_RETURNVALUE_OK;
}

Dummy_Read_ReturnValue_t Dummy_Read_Process(const int *input_buffer, int alsa_frame_count)
{
	bool dummy_flag;
	pthread_mutex_lock(&dummy_read_handler.dummy_read_mutex);
	dummy_flag = dummy_read_handler.dummy_flag;
	pthread_mutex_unlock(&dummy_read_handler.dummy_read_mutex);
	if (dummy_flag == false)
	{
		return DUMMY_READ_RETURNVALUE_OK;
	}

	#ifdef DUMMY_READ_PROCESS_ASSERT
	if (input_buffer == NULL)
	{
		syslog(LOG_ERR, "%s: Invalid input_buffer\n", __func__);
		return DUMMY_READ_RETURNVALUE_ERROR;
	}

	if (alsa_frame_count > dummy_read_handler.dummy_max_alsa_frame_count)
	{
		syslog(LOG_ERR, "%s: alsa_frame_count %d should be less than %d\n", __func__, alsa_frame_count, \
												dummy_read_handler.dummy_max_alsa_frame_count);
		alsa_frame_count = dummy_read_handler.dummy_max_alsa_frame_count;
	}
	#endif

	int i, j;
	/* put input frame date into stage queue */
	for (i = 0; i < alsa_frame_count * DUMMY_READ_INPUT_CHANNLENUM; i++)
	{
			EnStageQueue(dummy_read_handler.dummy_stage_queue, input_buffer[i]);
	}

	while(QueryStageQueue(dummy_read_handler.dummy_stage_queue) >= DUMMY_READ_PROCESS_SIZE_PERCYCLE)
	{
		/* pop date from stage queue */
		for(i = 0; i < DUMMY_READ_PROCESS_SIZE_PERCYCLE; i++)
		{
			DeStageQueue(dummy_read_handler.dummy_stage_queue, dummy_read_handler.dummy_stage_buffer + i);
		}

		/* Convert Bitwidth from 32-bit to 16-bit */
		for (i = 0; i < DUMMY_PROCESS_FRAME_COUNT ; i++)
		{
			dummy_read_handler.dummy_reformat_buffer[DUMMY_PROCESS_FRAME_COUNT * 0 + i] = (short)((dummy_read_handler.dummy_stage_buffer[i * 8 + 0] >> 14) & 0x0000ffff);
			dummy_read_handler.dummy_reformat_buffer[DUMMY_PROCESS_FRAME_COUNT * 1 + i] = (short)((dummy_read_handler.dummy_stage_buffer[i * 8 + 1] >> 14) & 0x0000ffff);
			dummy_read_handler.dummy_reformat_buffer[DUMMY_PROCESS_FRAME_COUNT * 2 + i] = (short)((dummy_read_handler.dummy_stage_buffer[i * 8 + 2] >> 14) & 0x0000ffff);
			dummy_read_handler.dummy_reformat_buffer[DUMMY_PROCESS_FRAME_COUNT * 3 + i] = (short)((dummy_read_handler.dummy_stage_buffer[i * 8 + 3] >> 14) & 0x0000ffff);
			dummy_read_handler.dummy_reformat_buffer[DUMMY_PROCESS_FRAME_COUNT * 4 + i] = (short)((dummy_read_handler.dummy_stage_buffer[i * 8 + 4] >> 14) & 0x0000ffff);
			dummy_read_handler.dummy_reformat_buffer[DUMMY_PROCESS_FRAME_COUNT * 5 + i] = (short)((dummy_read_handler.dummy_stage_buffer[i * 8 + 5] >> 14) & 0x0000ffff);
			dummy_read_handler.dummy_reformat_buffer[DUMMY_PROCESS_FRAME_COUNT * 6 + i] = (short)((dummy_read_handler.dummy_stage_buffer[i * 8 + 7] >> 16) & 0x0000ffff);
		}
	#ifdef DUMMY_FILE_BEFORE_RESAMPLE
		fwrite( &dummy_read_handler.dummy_reformat_buffer[DUMMY_PROCESS_FRAME_COUNT * 0], 2, DUMMY_PROCESS_FRAME_COUNT, fp1);
	#endif
		for (i = 0; i < DUMMY_READ_OUTPUT_CHANNLENUM; i++)
		{
			/* Convert Sample Rate from 48KHz to 16KHz */
			WebRtcSpl_Resample48khzTo16khz(dummy_read_handler.dummy_reformat_buffer + DUMMY_PROCESS_FRAME_COUNT * i, \
				dummy_read_handler.dummy_output_buffer, dummy_read_handler.dummy_resampler_handler[i], \
				dummy_read_handler.dummy_resampler_ram_buffer, DUMMY_PROCESS_FRAME_COUNT, DUMMY_PROCESS_FRAME_COUNT / 3);
			/* Write resampled data into queues*/
			for (j = 0; j < DUMMY_PROCESS_FRAME_COUNT / 3; j++)
			{
				pthread_mutex_lock(&dummy_read_handler.dummy_read_mutex_queue);
				EnQueue(dummy_read_handler.dummy_queue[i], *(dummy_read_handler.dummy_output_buffer+j));
				pthread_mutex_unlock(&dummy_read_handler.dummy_read_mutex_queue);
			}
		}
	}
	return DUMMY_READ_RETURNVALUE_OK;
}

#ifdef __cplusplus
}
#endif
