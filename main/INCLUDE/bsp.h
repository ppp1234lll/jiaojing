#ifndef _BSP_H_
#define _BSP_H_

#include "sys.h"
#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include "appconfig.h"
#include "SEGGER_RTT.h"

#define SYSTEM_SUPPORT_OS 	  (1)	 // 操作系统支持


#define PRIORITY_CONNECTION_MODE 1 // 0-有线优先连接 1-无线优先连接

#if (PRIORITY_CONNECTION_MODE == 1) 
#define WIRELESS_PRIORITY_CONNECTION // 无线优先连接
#else
#define WIRED_PRIORITY_CONNECTION    // 有线优先连接
#endif

/* Communication data processing mode */
#define COMDATA_PROCESS_MODE  (2) // 1-模式1（2020） 2-模式2（20210121）

#if (COMDATA_PROCESS_MODE == 1) 
#define COMDATA_PROCESS_MODE1 // 模式1
#else 
#define COMDATA_PROCESS_MODE2 // 模式2
#endif

//#define Enable_RTTViewer

//#ifdef Enable_RTTViewer
//#define LOG_P(...) do { SEGGER_RTT_SetTerminal(0);   \
//		                    SEGGER_RTT_printf(0, __VA_ARGS__); \
//                        }while(0);
//#endif


#endif

