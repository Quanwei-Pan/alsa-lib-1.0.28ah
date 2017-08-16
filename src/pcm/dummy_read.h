#ifndef _DUMMY_READ_H
#define _DUMMY_READ_H

#ifdef __cplusplus
extern "C" {
#endif
/*==================================================================================================

    Module Name: dummy_read.h

   General Description: This file defines the used v.

   ====================================================================================================

                               Xiaomi Confidential Proprietary
                        (c) Copyright Xiaomi 2017, All Rights Reserved


   Revision History:
                            Modification
   Author                          Date        Description of Changes
   -------------------------   ------------    -------------------------------------------
   Pan Quanwei                  16/08/2017      New code format for this C source file.


   ====================================================================================================
                                        INCLUDE FILES
   ==================================================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>


struct dummy_read_t {
 int i;
 FILE *fp;
} dummy_t;


#ifdef __cplusplus
}
#endif

#endif
