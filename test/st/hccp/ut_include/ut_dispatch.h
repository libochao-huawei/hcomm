#ifndef __UT_DISPATCH_H
#define __UT_DISPATCH_H

#include <stdio.h>
//#include <stdlib.h>

typedef	int (*stub_fn_t)(long unsigned int data0, long unsigned int data1, long unsigned int data2, long unsigned int data3, long unsigned int data4, long unsigned int data5);
typedef	long unsigned int (*stub_u64_fn_t)(long unsigned int data0, long unsigned int data1, long unsigned int data2, long unsigned int data3, long unsigned int data4, long unsigned int data5);

void ut_expect_int_eq(int orig, int value, const char *file, int line);
void ut_expect_int_ne(int orig, int value, const char *file, int line);
void ut_expect_int_lt(int orig, int value, const char *file, int line);
void ut_expect_int_le(int orig, int value, const char *file, int line);
void ut_expect_int_gt(int orig, int value, const char *file, int line);
void ut_expect_int_ge(int orig, int value, const char *file, int line);
void ut_expect_long_eq(long orig, long value, const char *file, int line);
void ut_expect_long_ne(long orig, long value, const char *file, int line);
void ut_expect_str_eq(char *a, char *b, const char *file, int line);
void ut_expect_str_ne(char *a, char *b, const char *file, int line);
void ut_expect_mem_eq(void *a, void *b, int s, const char *file, int line);
void ut_expect_addr_eq(void* orig, void* value, const char *file, int line);
void ut_expect_addr_ne(void* orig, void* value, const char *file, int line);
void ut_assert_int_eq(int orig, int value, const char *file, int line);
void ut_assert_int_ne(int orig, int value, const char *file, int line);
void ut_assert_addr_eq(void* orig, void* value, const char *file, int line);
void ut_assert_addr_ne(void* orig, void* value, const char *file, int line);
void ut_assert_int_true(int orig, const char *file, int line);

#define ASSERT_INT_EQ(orig, value)		ut_assert_int_eq(orig, value, __FILE__, __LINE__)
#define ASSERT_INT_NE(orig, value)		ut_assert_int_ne(orig, value, __FILE__, __LINE__)
#define ASSERT_ADDR_EQ(orig, value)		ut_assert_addr_eq(orig, value, __FILE__, __LINE__)
#define ASSERT_ADDR_NE(orig, value)		ut_assert_addr_ne(orig, value, __FILE__, __LINE__)
#define ASSERT_TRUE(orig)			ut_assert_int_true(orig, __FILE__, __LINE__)

#define EXPECT_INT_EQ(orig, value)		ut_expect_int_eq(orig, value, __FILE__, __LINE__)
#define EXPECT_INT_NE(orig, value)		ut_expect_int_ne(orig, value, __FILE__, __LINE__)
#define EXPECT_INT_LT(orig, value)		ut_expect_int_lt(orig, value, __FILE__, __LINE__)
#define EXPECT_INT_LE(orig, value)		ut_expect_int_le(orig, value, __FILE__, __LINE__)
#define EXPECT_INT_GT(orig, value)		ut_expect_int_gt(orig, value, __FILE__, __LINE__)
#define EXPECT_INT_GE(orig, value)		ut_expect_int_ge(orig, value, __FILE__, __LINE__)
#define EXPECT_LONG_EQ(orig, value)		ut_expect_long_eq(orig, value, __FILE__, __LINE__)
#define EXPECT_LONG_NE(orig, value)		ut_expect_long_ne(orig, value, __FILE__, __LINE__)
#define EXPECT_STR_EQ(orig, value)		ut_expect_str_eq(orig, value, __FILE__, __LINE__)
#define EXPECT_STR_NE(orig, value)		ut_expect_str_ne(orig, value, __FILE__, __LINE__)
#define EXPECT_MEM_EQ(orig, value, size)	ut_expect_mem_eq(orig, value, size, __FILE__, __LINE__)
#define EXPECT_ADDR_EQ(orig, value)		ut_expect_addr_eq((void *)orig, (void *)value, __FILE__, __LINE__)
#define EXPECT_ADDR_NE(orig, value)		ut_expect_addr_ne((void *)orig, (void *)value, __FILE__, __LINE__)


void mocker(stub_fn_t h , int most, int ret);

void mocker_ret(stub_fn_t h , int ret0, int ret1, int ret2);

void mocker_invoke(stub_fn_t sh , stub_fn_t th, int most);

void mocker_u64_invoke(stub_u64_fn_t sh , stub_u64_fn_t th, int most);
	
void mocker_clean();

#define TEST_M(T, f)	\
	TEST_F(T, st_##f)	\
	{    	\
	    f();	\
	}
	
#endif
