//=====================================================================
//
// The platform independence system call wrapper, Skywind 2004
// Unix/Windows 标准系统动态链接库编程通用接口
//
// HISTORY:
// Nov. 15 2004   skywind  created
//
// NOTE：
// 提供使 Unix或者 Windows相同的系统调用编程接口主要有几个方面的包装，
// 第一是时钟、第二是线程、第三是互斥，第四是进程，第五是进程通信，
// 第六// 是动态挂接动态连接，此部分实现动态连接，取名apr意再模仿
// apache的aprlib
//
//=====================================================================

#ifndef __APR_MOD_H__
#define __APR_MOD_H__

#include "aprsys.h"

// 动态连接库的操作
typedef void* apr_module;

#ifndef APR_MODULE
#ifdef __unix
#define APR_MODULE(type)  type 
#elif defined(_WIN32)
#define APR_MODULE(type)  __declspec(dllexport) type
#else
#error APR-MODULE only can be compiled under unix or win32 !!
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

APR_DECLARE(long)  apr_module_open(apr_module *module, const char *mod_file);
APR_DECLARE(long)  apr_module_close(apr_module module);
APR_DECLARE(void*) apr_module_symbol(apr_module module, const char *entry);

#ifdef __cplusplus
}
#endif

#endif

