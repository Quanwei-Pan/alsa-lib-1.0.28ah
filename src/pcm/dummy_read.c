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

====================================================================================================
                                        INCLUDE FILES
==================================================================================================*/
#include <stdio.h>
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

#define DUMMY_READ_OUTPUT_SAMPLERATE			(16000)
#define DUMMY_READ_OUTPUT_CHANNLENUM			(7)
#define DUMMY_READ_OUTPUT_BYTEWIDTH				(2)
#define DUMMY_READ_OUTPUT_SIZE_INBYTE_PERSECOND	(DUMMY_READ_OUTPUT_SAMPLERATE * \
												DUMMY_READ_OUTPUT_CHANNLENUM * \
												DUMMY_READ_OUTPUT_BYTEWIDTH)

#define DUMMY_READ_PROCESS_ASSERT
/*==================================================================================================
						Static variables / structure and other definations
==================================================================================================*/
typedef struct
{
	short *pBase;
	short pos;
	int maxsize;
}QUEUE, *PQUEUE;

typedef struct
{
	FILE *dummy_file;
	bool dummy_flag;
	bool dummy_file_flag;
	int dummy_file_size_inbyte;
	int dummy_buffer_size_inbyte;
	int dummy_frame_size_inbyte;
	int dummy_record_time;
	int dummy_queue_size_inbyte;
	short *dummy_reformat_buffer;
	int *dummy_resampler_ram_buffer;
	short *dummy_output_buffer;
	PQUEUE dummy_queue;
	char *dummy_file_name;
	WebRtcSpl_State48khzTo16khz *dummy_resampler_handler[DUMMY_READ_OUTPUT_CHANNLENUM];
}Dummy_Read_Handler_t;

static Dummy_Read_Handler_t dummy_read_handler = {
	.dummy_flag = false,
	.dummy_file_flag = false,
	.dummy_reformat_buffer = NULL,
	.dummy_resampler_ram_buffer = NULL,
	.dummy_output_buffer = NULL,
	.dummy_queue = NULL,
	.dummy_file_name = "/tmp/dummy_read.pcm",
	.dummy_file = NULL,
	.dummy_record_time = 0,
	.dummy_frame_size_inbyte = 0,
	.dummy_file_size_inbyte = 0,
	.dummy_queue_size_inbyte = 0
};
static WebRtcSpl_State48khzTo16khz resampler[DUMMY_READ_OUTPUT_CHANNLENUM];
static QUEUE Dummy_Read_Queue;
//int dummy_flag = 0;
/*==================================================================================================
                                      STATIC FUNCTIONS
==================================================================================================*/
static Dummy_Read_ReturnValue_t CreateQueue(PQUEUE Q, int maxsize)
{
	Q->pBase = (short *)malloc(sizeof(short)*maxsize);
	if (NULL == Q->pBase)
	{
		printf("%s: Memory allocate failed for %d bytes \n", __func__, maxsize);
		return DUMMY_READ_RETURNVALUE_ERROR;
	}
	memset(Q->pBase, 0, maxsize * sizeof(short));
	Q->maxsize = maxsize;
	return DUMMY_READ_RETURNVALUE_OK;
}

static Dummy_Read_ReturnValue_t FreeQueue(PQUEUE Q)
{
	free(Q->pBase);
	return DUMMY_READ_RETURNVALUE_OK;
}

static Dummy_Read_ReturnValue_t Enqueue(PQUEUE Q, short val)
{
	Q->pBase[Q->pos] = val;
	Q->pos = (Q->pos + 1) % Q->maxsize;
	return DUMMY_READ_RETURNVALUE_OK;
}
/*==================================================================================================
                                      GLOBAL FUNCTIONS
==================================================================================================*/
Dummy_Read_ReturnValue_t Dummy_Read_Init(char * file_name, int mem_size_inbyte)
{
	dummy_read_handler.dummy_queue_size_inbyte = mem_size_inbyte;
	dummy_read_handler.dummy_frame_size_inbyte = 32;  //

	strcpy(dummy_read_handler.dummy_file_name, file_name);

	dummy_read_handler.dummy_queue = &Dummy_Read_Queue;
	CreateQueue(dummy_read_handler.dummy_queue, dummy_read_handler.dummy_queue_size_inbyte);

	/* Init resampler for each channel */
	int i;
	for (i = 0; i < DUMMY_READ_OUTPUT_CHANNLENUM; i++)
	{
		dummy_read_handler.dummy_resampler_handler[i] = &resampler[i];
		WebRtcSpl_ResetResample48khzTo16khz(dummy_read_handler.dummy_resampler_handler[i]);
	}

	return DUMMY_READ_RETURNVALUE_OK;
}

Dummy_Read_ReturnValue_t Dummy_Read_Finalize(void)
{

}

Dummy_Read_ReturnValue_t Dummy_Read_Set_Trigger(bool enable)
{
    if(enable == true)
    {
        printf("Alsa audio hack has opened!\n");
        dummy_read_handler.dummy_flag = true;
        return DUMMY_READ_RETURNVALUE_OK;
    }
    else if(enable == false)
    {
        printf("Alsa audio hack has closed!\n");
        dummy_read_handler.dummy_flag = false;
        return DUMMY_READ_RETURNVALUE_OK;
    }
    else
        return DUMMY_READ_RETURNVALUE_ERROR;
}

Dummy_Read_ReturnValue_t Dummy_Read_Generate_File(int time_in_sec)
{

	if (time_in_sec <= 0)
	{
		printf("%s: Invalid time input %d sec \n", __func__, time_in_sec);
		return DUMMY_READ_RETURNVALUE_OK;
	}
	if (time_in_sec > dummy_read_handler.dummy_queue_size_inbyte / DUMMY_READ_OUTPUT_SIZE_INBYTE_PERSECOND)
	{
		printf("%s: Time required %d sec is larger than the queue size %d\n", __func__, time_in_sec, \
			dummy_read_handler.dummy_queue_size_inbyte / DUMMY_READ_OUTPUT_SIZE_INBYTE_PERSECOND);
		return DUMMY_READ_RETURNVALUE_OK;
	}

	if (dummy_read_handler.dummy_file_flag == false)
	{
		dummy_read_handler.dummy_file_flag = true;
		dummy_read_handler.dummy_file_size_inbyte = time_in_sec * DUMMY_READ_OUTPUT_SIZE_INBYTE_PERSECOND;
		return DUMMY_READ_RETURNVALUE_OK;
	}
	else
	{
		printf("%s: Last time is not done yet\n", __func__);
		return DUMMY_READ_RETURNVALUE_ERROR;
	}
}

Dummy_Read_ReturnValue_t Dummy_Read_Process(const int *input_buffer, int size_inbyte)
{
	if(dummy_read_handler.dummy_flag == false)
	{
		return DUMMY_READ_RETURNVALUE_OK;
	}
#ifdef DUMMY_READ_PROCESS_ASSERT
	if (input_buffer == NULL)
	{
		printf("%s: Invalid input_buffer\n", __func__);
		return DUMMY_READ_RETURNVALUE_ERROR;
	}

//	if (size_inbyte < DUMMY_READ_INPUT_SIZE_INBYTE_PERSECOND)
//	{
//		printf("%s: Invalid size_inbyte %d should be larger than %d\n", __func__, size_inbyte, DUMMY_READ_INPUT_SIZE_INBYTE_PERSECOND);
//		return DUMMY_READ_RETURNVALUE_ERROR;
//	}
#endif

	int alsa_frame_count = 1024;
	int i, j;

	/* Convert Bitwidth from 32-bit to 16-bit */
	for (i = 0; i < alsa_frame_count; i++)
	{
		dummy_read_handler.dummy_reformat_buffer[alsa_frame_count * 0 + i] = (short)((*input_buffer++ >> 14) & 0x0000ffff);
		dummy_read_handler.dummy_reformat_buffer[alsa_frame_count * 1 + i] = (short)((*input_buffer++ >> 14) & 0x0000ffff);
		dummy_read_handler.dummy_reformat_buffer[alsa_frame_count * 2 + i] = (short)((*input_buffer++ >> 14) & 0x0000ffff);
		dummy_read_handler.dummy_reformat_buffer[alsa_frame_count * 3 + i] = (short)((*input_buffer++ >> 14) & 0x0000ffff);
		dummy_read_handler.dummy_reformat_buffer[alsa_frame_count * 4 + i] = (short)((*input_buffer++ >> 14) & 0x0000ffff);
		dummy_read_handler.dummy_reformat_buffer[alsa_frame_count * 5 + i] = (short)((*input_buffer++ >> 14) & 0x0000ffff);
		input_buffer++;
		dummy_read_handler.dummy_reformat_buffer[alsa_frame_count * 6 + i] = (short)((*input_buffer++ >> 16) & 0x0000ffff);
//		printf("%s: input buf = %0x4x %0x4x %0x4x %0x4x %0x4x %0x4x %0x4x %0x4x\n", __func__, input_buffer[i*8], input_buffer[i*8+1], \
//			input_buffer[i*8+2], input_buffer[i*8+3], input_buffer[i*8+4], input_buffer[i*8+5], input_buffer[i*8+6], input_buffer[i*8+7]);
	}

	for (i = 0; i < DUMMY_READ_OUTPUT_CHANNLENUM; i++)
	{
		/* Convert Sample Rate from 48KHz to 16KHz */
		WebRtcSpl_Resample48khzTo16khz(dummy_read_handler.dummy_reformat_buffer + alsa_frame_count * i, dummy_read_handler.dummy_output_buffer, \
			dummy_read_handler.dummy_resampler_handler[i], dummy_read_handler.dummy_resampler_ram_buffer, alsa_frame_count, alsa_frame_count / 3);

		/* Store output into the Queue */
		for (j = 0; j < alsa_frame_count; j++)
		{
			Enqueue(dummy_read_handler.dummy_queue, dummy_read_handler.dummy_output_buffer[j]);
		}
	}

	/* Write mic data into file */
	if (dummy_read_handler.dummy_file_flag == true)
	{

		FILE * fp = fopen(dummy_read_handler.dummy_file_name,"ab+");
		int head = (dummy_read_handler.dummy_queue->pos - dummy_read_handler.dummy_file_size_inbyte + \
					dummy_read_handler.dummy_queue_size_inbyte) \
					% dummy_read_handler.dummy_queue_size_inbyte;
		if (dummy_read_handler.dummy_queue->pos >=  dummy_read_handler.dummy_file_size_inbyte)
		{
			fwrite(dummy_read_handler.dummy_queue->pBase + dummy_read_handler.dummy_queue->pos - \
					dummy_read_handler.dummy_file_size_inbyte, \
				 1, dummy_read_handler.dummy_file_size_inbyte, fp);
		}
		else
		{
			fwrite(dummy_read_handler.dummy_queue->pBase + dummy_read_handler.dummy_queue->pos - \
				dummy_read_handler.dummy_file_size_inbyte + dummy_read_handler.dummy_queue_size_inbyte, \
				 1, dummy_read_handler.dummy_file_size_inbyte - dummy_read_handler.dummy_queue->pos, fp);
			 fwrite(dummy_read_handler.dummy_queue->pBase, 1, dummy_read_handler.dummy_queue->pos, fp);
		}
		fclose(fp);
		dummy_read_handler.dummy_file_flag = false;
	}
	return DUMMY_READ_RETURNVALUE_OK;

}

#ifdef __cplusplus
}
#endif
