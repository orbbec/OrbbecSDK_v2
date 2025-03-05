#ifndef __DEBUG_LOG_H__
#define __DEBUG_LOG_H__

#include <stdio.h>
#include <stdint.h>
#include "stdlib.h"

#define DEBUG   1

#define Print_TAG		"OCL_DEV_API"


#if DEBUG
#define Debug_info(fmt,...)      {printf("%s info    -[fun:%-25.40s line:%05d:	]:",Print_TAG,__FUNCTION__,__LINE__);printf(fmt,##__VA_ARGS__);}
#else
#define Debug_info(fmt,...)
#endif


#define Debug_log(fmt,...)      {printf("%s log    -[fun:%-25.40s line:%05d:	]:",Print_TAG,__FUNCTION__,__LINE__);printf(fmt,##__VA_ARGS__);}
#define Debug_error(fmt,...)    {printf("%s error  -[fun:%-25.40s line:%05d:	]:",Print_TAG,__FUNCTION__,__LINE__);printf(fmt,##__VA_ARGS__);}
#define Debug_warning(fmt,...)  {printf("%s warning-[fun:%-25.40s line:%05d:	]:",Print_TAG,__FUNCTION__,__LINE__);printf(fmt,##__VA_ARGS__);}

//#define ENABALE_ALL_LOG

#ifndef ENABALE_ALL_LOG
#define DISABLE_ERR_LOG
#define DISABLE_CND_LOG
#define DISABLE_FUN_LOG
#define DISABLE_INFO_LOG
#define DISABLE_PRT_LOG
#define DISABLE_CTL_LOG
#define DISABLE_FORMAT_LOG
#define DISABLE_DBG_LOG
#define DISABLE_WARNING_LOG
#endif

#define  dbg_printf fprintf
#define  log_level  stderr
#define  cam_get_task_id()  ((uint32_t)pthread_self())


#ifndef DISABLE_ERR_LOG
#define log_err(fmt,...) do{\
    dbg_printf(log_level,"[%x][ERR] %s " fmt "\n",cam_get_task_id(),__FUNCTION__,##__VA_ARGS__);\
    }while(0)
#else
#define log_err(fmt,...) do{}while(0)
#endif

#ifndef DISABLE_CND_LOG
#define log_conditon(fmt, ...) do{\
    dbg_printf(log_level,"[%x][CND] %s "fmt"\n",cam_get_task_id(),__FUNCTION__,##__VA_ARGS__);\
    }while(0)
#else
#define log_conditon(fmt, ...) do{}while(0)
#endif


#ifndef DISABLE_FUN_LOG
#define log_func_enter() do{\
    dbg_printf(log_level,"[%x][FUNC] %s enter %d\n",cam_get_task_id(),__FUNCTION__,__LINE__);\
    }while(0)
#define log_func_exit() do{\
    dbg_printf(log_level,"[%x][FUNC] %s exit\n",cam_get_task_id(),__FUNCTION__);\
    }while(0)
#else
#define log_func_enter() do{}while(0)
#define log_func_exit() do{}while(0)
#endif


#ifndef DISABLE_INFO_LOG
#define log_info(fmt, ...) do{\
    dbg_printf(log_level,"[%x][INFO] %s "fmt"\n",cam_get_task_id(),__FUNCTION__,##__VA_ARGS__);\
    }while(0)
#else
#define log_info(fmt, ...) do{}while(0)
#endif


#ifndef DISABLE_DBG_LOG
#define log_debug(fmt, ...) do{\
    dbg_printf(log_level,"[%x][DEBUG] %s "fmt"\n",cam_get_task_id(),__FUNCTION__,##__VA_ARGS__);\
    }while(0)
#else
#define log_debug(fmt, ...)  do{}while(0)
#endif


#ifndef DISABLE_PRT_LOG
#define log_printf(fmt, ...) do{\
    dbg_printf(log_level,""fmt"",##__VA_ARGS__);\
    }while(0)
#else
#define log_printf(fmt, ...) do{}while(0)
#endif


#ifndef DISABLE_CTL_LOG
#define log_ctl(ctl_code)    do{\
    dbg_printf(log_level,"[%x][CTL] %s set ctl %s=%d\n",cam_get_task_id(),__FUNCTION__,log_get_string(ctl_code),ctl_code);\
    }while(0)
#else
#define log_ctl(ctl_code) do{}while(0)
#endif


#ifndef DISABLE_FORMAT_LOG
#define log_format(format)    do{\
    dbg_printf(log_level,"[%x][FORMAT] %s the stream type is %s=%d\n",cam_get_task_id(),__FUNCTION__,log_get_stream_type_str(format),format);\
    }while(0)
#else
#define log_format(format) do{}while(0)
#endif

#ifndef DISABLE_ALWAYS_LOG
#define log_always(fmt, ...)    do{\
        dbg_printf(log_level,"[%x][ALWAYS] %s " fmt "\n",cam_get_task_id(),__FUNCTION__,##__VA_ARGS__);\
        }while(0)
#else
#define log_always(fmt, ...) do{}while(0)
#endif

#ifndef DISABLE_WARNING_LOG
#define log_warning(fmt, ...)    do{\
        dbg_printf(log_level,"[%x][WARN] %s "fmt"\n",cam_get_task_id(),__FUNCTION__,##__VA_ARGS__);\
        }while(0)
#else
#define log_warning(fmt, ...) do{}while(0)
#endif


#endif