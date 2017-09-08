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
Quanwei Pan                  09/05/2017     Add opus codec
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
	NUM_OF_DUMMY_READ_RETURNVALUE
}Dummy_Read_ReturnValue_t;

/*==================================================================================================
                                     FUNCTION PROTOTYPES
==================================================================================================*/
/*
 +------------------------------------------------------------------------------+
 |  函数名称：Dummy_Read_Init                                                     |
 |  函数作用：mic数据采集模块初始化                                                  |
 |  输入参数：char * file_name 文件保存的地址，如”/tmp/dummy_read.pcm”       	       |
 |            int mem_size_inbyte 音频缓存大小（可占用内存），单位byte，     	     |
 |                                降采样后每一秒数据大小为224Kbytes          	     |
 |  返回值：  DUMMY_READ_RETURNVALUE_OK，设置成功                            	  |
 |            DUMMY_READ_RETURNVALUE_ERROR，设置失败                         	 |
 |  函数说明：只能在程序初始化时调用                                         	      |
 +------------------------------------------------------------------------------+
*/
Dummy_Read_ReturnValue_t Dummy_Read_Init(char * file_name, int mem_size_inbyte);
/*
 +------------------------------------------------------------------------------+
 |  函数名称：Dummy_Read_Finalize                                            	 |
 |  函数作用：mic数据采集模块资源释放                                        	      |
 |  输入参数：无                                                             	    |
 |  返回值：  DUMMY_READ_RETURNVALUE_OK，设置成功                            	  |
 |            DUMMY_READ_RETURNVALUE_ERROR，设置失败                         	 |
 |  函数说明：无                                                             	    |
 +------------------------------------------------------------------------------+
*/
Dummy_Read_ReturnValue_t Dummy_Read_Finalize(void);
/*
 +------------------------------------------------------------------------------+
 |  函数名称：Dummy_Read_Set_Trigger                                         	 |
 |  函数作用：mic数据采集开关                                                	     |
 |  输入参数：bool enable                                                    	 |
 |  返回值：  DUMMY_READ_RETURNVALUE_OK，设置成功                            	  |
 |            DUMMY_READ_RETURNVALUE_ERROR，设置失败                         	 |
 |  函数说明：无                                                            	    |
 +------------------------------------------------------------------------------+
*/
Dummy_Read_ReturnValue_t Dummy_Read_Set_Trigger(bool enable);
/*
 +------------------------------------------------------------------------------+
 |  函数名称：Dummy_Read_Generate_File                                       	 |
 |  函数作用：生成指定时间长度的音频文件，从当前时刻倒推                     	           |
 |  输入参数：int time_in_sec 指定时间长度                                   	     |
 |  返回值：  DUMMY_READ_RETURNVALUE_OK，设置成功                            	  |
 |            DUMMY_READ_RETURNVALUE_ERROR，设置失败，指定时间大于缓存容量   	     |
 |  函数说明：无                                                             	    |
 +------------------------------------------------------------------------------+
*/
Dummy_Read_ReturnValue_t Dummy_Read_Generate_File(int time_in_sec);
/*
 +------------------------------------------------------------------------------+
 |  函数名称：Dummy_Read_Process                                             	 |
 |  函数作用：实现mic数据采集到缓存区                                        	      |
 |  输入参数：const int * input_buffer 输入mic数据                           	  |
 |            int alsa_frame_count 输入mic数据的alsa帧数                     	   |
 |  返回值：  DUMMY_READ_RETURNVALUE_OK，设置成功                            	  |
 |            DUMMY_READ_RETURNVALUE_ERROR，设置失败                         	 |
 |  函数说明：输入mic数据为const类型，确保不被修改                           	       |
 +------------------------------------------------------------------------------+
*/
Dummy_Read_ReturnValue_t Dummy_Read_Process(const int * input_buffer, int alsa_frame_count);

#endif

#ifdef __cplusplus
}
#endif
