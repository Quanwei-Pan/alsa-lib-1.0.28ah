#ifdef __cplusplus
extern "C" {
#endif

#ifndef DUMMY_READ_H
#define DUMMY_READ_H
/*==================================================================================================

     Header Name: dummy_read.h

     General Description: This file defines the API of dummy_read module.

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
#include <stdbool.h>

/*==================================================================================================
										CONSTANTS
==================================================================================================*/


/*==================================================================================================
						Static variables / structure and other definations
==================================================================================================*/


/*==================================================================================================
								GLOBAL VARIABLE DECLARATIONS
==================================================================================================*/
typedef enum
{
	DUMMY_READ_RETURNVALUE_ERROR = -1,
	DUMMY_READ_RETURNVALUE_OK = 0,
	NUM_OF_DUMMY_READ_RETURNVALUE,
}Dummy_Read_ReturnValue_t;

/*==================================================================================================
                                     FUNCTION PROTOTYPES
==================================================================================================*/
//APIs For App
Dummy_Read_ReturnValue_t Dummy_Read_Init(char * file_name, int mem_size_inbyte);
Dummy_Read_ReturnValue_t Dummy_Read_Finalize(void);
Dummy_Read_ReturnValue_t Dummy_Read_Set_Trigger(bool enable);
Dummy_Read_ReturnValue_t Dummy_Read_Generate_File(int time_in_sec);

//APIs For Alsa-lib Read
Dummy_Read_ReturnValue_t Dummy_Read_Process(const int * input_buffer, int alsa_frame_count);

#endif

#ifdef __cplusplus
}
#endif
