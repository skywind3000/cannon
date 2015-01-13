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

#include "aprmod.h"

#include <stdio.h>

#ifdef __unix
#include <dlfcn.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#error APR-MODULE only can be compiled under unix or win32 !!
#endif

APR_DECLARE(long) apr_module_open(apr_module *module, const char *mod_file)
{
	void *handle;

	if (module == NULL) return -1;
	#ifdef __unix
	handle = dlopen(mod_file, RTLD_LAZY);
	#else
	handle = (void*)LoadLibraryA(mod_file);
	#endif
	*module = handle;

	return 0;
}

APR_DECLARE(long) apr_module_close(apr_module module)
{
	int retval = 0;
	#ifdef __unix
	retval = dlclose(module);
	#else
	retval = FreeLibrary((HINSTANCE)module);
	#endif
	return (long)retval;
}

APR_DECLARE(void*) apr_module_symbol(apr_module module, const char *entry)
{
	void* startp = NULL;
	#ifdef __unix
	startp = dlsym(module, entry);
	#else
	startp = (void*)GetProcAddress((HINSTANCE)module, entry);
	#endif
	return startp;
}


