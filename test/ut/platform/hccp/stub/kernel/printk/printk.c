
#include "stdio.h"
#include <stdarg.h>


#define _stub_print_va_list(fmt) ({\
	int ret = 0;\
	if (fmt) {	\
		va_list ap; \
		va_start(ap, fmt); \
		ret = vprintf(fmt, ap); \
		va_end(ap); \
	} \
	ret;\
})


int printk(const char *fmt, ...)
{
	return _stub_print_va_list(fmt);
}

void warn_slowpath_fmt(const char *file, const int line,
		       const char *fmt, ...)
{
	printk("warn_slowpath : %s:%d:", file, line);
	_stub_print_va_list(fmt);
}	
