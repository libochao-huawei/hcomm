#ifndef __LLT_DISPATCH_H
#define __LLT_DISPATCH_H

#include <stdio.h>


#ifdef CONFIG_LLT_ST
#define TEST_M(T, f)	\
	TEST_F(T, st_##f)	\
	{    	\
	    f();	\
	}
#else
#define TEST_M(T, f)	\
	TEST_F(T, ut_##f)	\
	{    	\
	    f();	\
	}
#endif
#endif
