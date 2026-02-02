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


int __warn_printk(const char *fmt, ...)
{
	return _stub_print_va_list(fmt);
}
