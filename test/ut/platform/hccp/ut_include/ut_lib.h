/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __UT_LIB_H__
#define __UT_LIB_H__


/* unit test common */
#define var_zero(a) memset(&a, 0,  sizeof(a))

#define __ut_stringify(...)	#__VA_ARGS__
#define ut_stringify(...)	__ut_stringify(__VA_ARGS__)""

/* unit test list */
struct ut_list_head {
	struct ut_list_head *next, *prev;
};

#define UT_LIST_HEAD_INIT(name) { &(name), &(name) }

static inline void ut_init_list_head(struct ut_list_head *list)
{
	list->next = list;
	list->prev = list;
}

static inline  void ut_list_add(struct ut_list_head *new_list, struct ut_list_head *head)
{
	head->next->prev = new_list;
	new_list->next = head->next;
	new_list->prev = head;
	head->next = new_list;
}

static inline void ut_list_del(struct ut_list_head *entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	entry->next = 0;
	entry->prev = 0;
}

#define ut_list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

int ut_printf (const char *fmt, ...);
#define SUPPORT_COLOR_TEXT

#ifdef SUPPORT_COLOR_TEXT
#define _CONSOLE_DISP_NONE                 "\e[0m"
#define _CONSOLE_DISP_BLACK                "\e[0;30m"
#define _CONSOLE_DISP_RED                  "\e[0;31m"
#define _CONSOLE_DISP_GREEN                "\e[0;32m"
#define _CONSOLE_DISP_BROWN                "\e[0;33m"
#define _CONSOLE_DISP_YELLOW               "\e[1;33m"
#define _CONSOLE_DISP_BLUE                 "\e[0;34m"
#define _CONSOLE_DISP_PURPLE               "\e[0;35m"
#define _CONSOLE_DISP_CYAN                 "\e[0;36m"
#define _CONSOLE_DISP_GRAY                 "\e[0;37m"
#define _CONSOLE_DISP_WHITE                "\e[1;37m"
#else
#define _CONSOLE_DISP_NONE                 ""
#define _CONSOLE_DISP_BLACK                ""
#define _CONSOLE_DISP_RED                  ""
#define _CONSOLE_DISP_GREEN                ""
#define _CONSOLE_DISP_BROWN                ""
#define _CONSOLE_DISP_YELLOW               ""
#define _CONSOLE_DISP_BLUE                 ""
#define _CONSOLE_DISP_PURPLE               ""
#define _CONSOLE_DISP_CYAN                 ""
#define _CONSOLE_DISP_GRAY                 ""
#define _CONSOLE_DISP_WHITE                ""
#endif

int llt_register_clock(void (*fn)(void *), int nsec, void *data, char *name);
#define LLT_CLOCK_ID_ALL -1
#define LLT_CLOCK_START 1
#define LLT_CLOCK_STOP  0
void llt_set_clockstate(int id, int state);

/* unit test map table for storing key-value pair */
struct ut_map {
	struct ut_list_head entrys;
	char *name;
};
/* unit test map operation flags */
#define _UT_KEYVAL_MAP_FLAG_TEST 0
#define _UT_KEYVAL_MAP_FLAG_READ 1
#define _UT_KEYVAL_MAP_FLAG_DEL  2
#define _UT_KEYVAL_MAP_FLAG_SIZE 3

struct ut_entry {
	int  id;
	void *key;
	void *data;
};
int ut_map_clear(struct ut_map *m, char *file, int line);
struct ut_entry *ut_map_insert(struct ut_map *m, const struct ut_entry *item, char * file, int line);
struct ut_entry *ut_map_erase(struct ut_map *m, struct ut_entry *item, char *file, int line);
struct ut_entry *ut_map_find(struct ut_map *m, struct ut_entry *item, int flags, char *file, int line);

/* init key-value pairs entry */
#define UT_ENTRY_INIT(n, k, v) { \
		.id		= (int)(n), \
		.key		= (void *)(k), \
		.data		= (void *)(v)}

#define UT_MAP_VAR(func,cls) __ut_map_##func##_##cls
#define UT_MAP_DECLARE(func,cls) \
	struct ut_map UT_MAP_VAR(func,cls)
	
#define UT_MAP_DEFINE(func,cls) UT_MAP_DECLARE(func,cls) = { \
	.name   = __ut_stringify(func) "(" __ut_stringify(cls) ")", \
	.entrys = UT_LIST_HEAD_INIT(UT_MAP_VAR(func,cls).entrys) }

#define UT_MAP_CLR(func,cls) ut_map_clear(&UT_MAP_VAR(func,cls), __FILE__, __LINE__)

#define UT_MAP_INSERT_WITH_ID(func,cls,k,v,n) ({ \
	struct ut_entry e = UT_ENTRY_INIT(n, k, v); \
	struct ut_entry *ptr_e; \
	ptr_e = ut_map_insert(&UT_MAP_VAR(func,cls), &e, __FILE__, __LINE__);\
	ptr_e ? ptr_e->data : NULL;})
	
#define UT_MAP_ERASE_WITH_ID(func,cls,k,n) ({ \
	struct ut_entry e = UT_ENTRY_INIT(n, k, NULL); \
	struct ut_entry *ptr_e;\
	ptr_e = ut_map_erase(&UT_MAP_VAR(func,cls), &e, __FILE__, __LINE__); \
	ptr_e ? ptr_e->data : NULL;})

#define UT_MAP_REPLACE_WITH_ID(func,cls,k,v,n) ({ \
	void *data = UT_MAP_ERASE_WITH_ID(func,cls,k,n); \
	UT_MAP_INSERT_WITH_ID(func,cls,k,v,n);\
	data;})

#define UT_MAP_EXIST_WITH_ID(func,cls,k,n) ({ \
	struct ut_entry e = UT_ENTRY_INIT(n, k, NULL); \
	struct ut_entry *ptr_e;\
	ptr_e = ut_map_find(&UT_MAP_VAR(func,cls), &e, _UT_KEYVAL_MAP_FLAG_TEST, __FILE__, __LINE__); \
	ptr_e ? 1 : 0;})

#define UT_MAP_FIND_WITH_ID(func,cls,k,n) ({ \
	struct ut_entry e = UT_ENTRY_INIT(n, k, NULL); \
	struct ut_entry *ptr_e;\
	ptr_e = ut_map_find(&UT_MAP_VAR(func,cls), &e, _UT_KEYVAL_MAP_FLAG_READ, __FILE__, __LINE__); \
	ptr_e ? ptr_e->data : NULL;})

#define UT_MAP_INSERT(func,cls,k,v) UT_MAP_INSERT_WITH_ID(func,cls,k,v,-1)
#define UT_MAP_ERASE(func,cls,k)    UT_MAP_ERASE_WITH_ID(func,cls,k,-1)
#define UT_MAP_REPLACE(func,cls,k,v)  UT_MAP_REPLACE_WITH_ID(func,cls,k,v,-1)
#define UT_MAP_EXIST(func,cls,k)    UT_MAP_EXIST_WITH_ID(func,cls,k,-1)
#define UT_MAP_FIND(func,cls,k)     UT_MAP_FIND_WITH_ID(func,cls,k,-1)

/* unit test counter range */
struct ut_cnt_range {
	int counter;
	char *name;
	struct ut_list_head ranges;
};
/* unit test cnt range operation flags */
#define _UT_CNT_RANGE_FLAG_TEST 0
#define _UT_CNT_RANGE_FLAG_READ 1
#define _UT_CNT_RANGE_FLAG_ADD  2
#define _UT_CNT_RANGE_FLAG_DEL  3
#define _UT_CNT_RANGE_FLAG_SIZE 4

void _ut_cnt_range_append(struct ut_cnt_range *range_handle, int start, int end, char *file, int line) ;
void _ut_cnt_range_clear(struct ut_cnt_range *counter_range, char *file, int line) ;
int _ut_cnt_range_check(struct ut_cnt_range *counter_range, int step, int *ptr_count, char *file, int line) ;

#define UT_CNT_RANGE_VAR(func,cls) __ut_range_##func##_##cls
#define UT_CNT_RANGE_DECLARE(func, cls) struct ut_cnt_range UT_CNT_RANGE_VAR(func, cls)

#define UT_CNT_RANGE_DEFINE(func, cls) UT_CNT_RANGE_DECLARE(func, cls) = { \
	.counter = 0, \
	.name    = __ut_stringify(func) "(" __ut_stringify(cls) ")", \
	.ranges  = UT_LIST_HEAD_INIT(UT_CNT_RANGE_VAR(func, cls).ranges) }

#define UT_CNT_RANGE_CHECK(func, cls, step) \
	_ut_cnt_range_check(&UT_CNT_RANGE_VAR(func, cls), step, NULL, __FILE__, __LINE__)

#define UT_CNT_RANGE_CHECK_AND_GET(func, cls, step, ptr_cnt) \
	_ut_cnt_range_check(&UT_CNT_RANGE_VAR(func, cls), step, ptr_cnt, __FILE__, __LINE__)

#define UT_CNT_RANGE_ADD(func, cls, start, end) \
	_ut_cnt_range_append(&UT_CNT_RANGE_VAR(func, cls), start, end, __FILE__, __LINE__)

#define UT_CNT_RANGE_CLR(func, cls) \
	_ut_cnt_range_clear(&UT_CNT_RANGE_VAR(func, cls),__FILE__, __LINE__)


/* compose trigger */
#define UT_CNT_MAP_ADD(func, cls, start, end, val) do { \
	int cnt = 0;	\
	UT_CNT_RANGE_ADD(func, cls, start, end);\
	for (cnt = start; cnt <= end; cnt++) \
		UT_MAP_INSERT(func, cls, cnt, val);\
} while(0)

#define UT_CNT_MAP_CLR(func, cls) do { \
	UT_CNT_RANGE_CLR(func, cls);\
	UT_MAP_CLR(func, cls);\
} while(0)

/* unit test malloc */
#define UT_MEM_FLAG_NOCHECK 0x00000001
#define UT_MEM_FLAG_ZERO    0x00000002
#define UT_MEM_FLAG_RANDOM  0x00000004

void *ut_mem_oob_data(void *p);
void *ut_malloc_trace(int size, int oob_size, int flags, 
		      const char *file, int line, const char *fmt, ...);
void ut_free_trace(void *p, int flags, const char *file, int line, const char *fmt, ...);
int ut_mem_error();
	
#define ut_zalloc(size) ut_malloc_trace(size, 0, UT_MEM_FLAG_ZERO, __FILE__, __LINE__, 0)
#define ut_malloc(size) ut_malloc_trace(size, 0, 0, __FILE__, __LINE__, 0)
#define ut_free(p) 	ut_free_trace(p, 0, __FILE__, __LINE__, 0)
#define ut_mfree(p, flags) 	ut_free_trace(p, flags, __FILE__, __LINE__, 0)

/* unit test function stub */
typedef	int (*ut_stub_fn_t)(void *data);
typedef	void * ut_stub_ret_t;

struct ut_stub_data {
	int type;
	char *name;
	union {
		ut_stub_fn_t func;
		ut_stub_ret_t ret;
	} val;
};

struct ut_stub_handle {
	ut_stub_fn_t func;
	struct ut_stub_data body;
	struct ut_stub_data init;
	struct ut_stub_data stub;
};

#define UT_STUB_TYPE_ENTRY   0x50
#define UT_STUB_TYPE_RETURN  0x51

/* init function entry stub */
#define UT_ENTRY_STUB_INIT(nm,fn) {		 \
		.type		= UT_STUB_TYPE_ENTRY, \
		.name		= ut_stringify(nm), \
		.val.func	= (ut_stub_fn_t)(fn) }

/* init function return stub */
#define UT_RETUEN_STUB_INIT(nm,value) {		 \
		.type		= UT_STUB_TYPE_RETURN, \
		.name		= ut_stringify(nm), \
		.val.ret	= (ut_stub_ret_t)(value) }


/* init stub for replace function entry to new */
#define UT_FUNC_ENTRY_STUB_INIT(origfunc, newfunc) { \
		.func	= (ut_stub_fn_t)origfunc, \
		.body	= UT_ENTRY_STUB_INIT(origfunc, origfunc), \
		.init	= UT_ENTRY_STUB_INIT(origfunc, origfunc), \
		.stub	= UT_ENTRY_STUB_INIT(newfunc, newfunc) }

/* init stub for replace function return value to other */
#define UT_FUNC_RETUEN_STUB_INIT(origfunc, retval) {		 \
		.func	= (ut_stub_fn_t)origfunc,	\
		.body	= UT_ENTRY_STUB_INIT(origfunc, origfunc), \
		.init	= UT_RETUEN_STUB_INIT(origfunc, retval), \
		.stub	= UT_RETUEN_STUB_INIT(retval, retval) }

#define UT_FUNC_RETUEN_STUB_INIT_WITH_AGENT(origfunc, agentfunc, retval) {		 \
		.func	= (ut_stub_fn_t)origfunc,	\
		.body	= UT_ENTRY_STUB_INIT(agentfunc, agentfunc), \
		.init	= UT_RETUEN_STUB_INIT(origfunc, retval), \
		.stub	= UT_RETUEN_STUB_INIT(origfunc, origfunc) }


/* stub agent define */
#define _UT_STUB_AGENT_RETURN(handler) _stub_agent##handler##_return
#define _UT_STUB_AGENT_ENTRY(handler)  _stub_agent##handler##_entry

#define UT_STUB_AGENT_DEFINE(name, rettype, retvalue) \
	static rettype _UT_STUB_AGENT_RETURN(name) = (rettype)(retvalue); \
	static rettype _UT_STUB_AGENT_ENTRY(name)(void *data) { \
		/*ut_printf("\n<agt" _CONSOLE_DISP_PURPLE " %s" _CONSOLE_DISP_NONE ">\n", __FUNCTION__);*/\
		return _UT_STUB_AGENT_RETURN(name); \
	}

int _ut_stub_register(struct ut_stub_handle *stub_handle, 
						struct ut_stub_data *newstub, char *f, int line);
void _ut_stub_unregister(struct ut_stub_handle *stub_handle, char * f, int line);

#define UT_ENTRY_STUB_HANDLE_DECLAER(name)	\
	struct ut_stub_handle name

#define UT_ENTRY_STUB_DEFINE(name, oldfunc, newfunc)		\
	UT_ENTRY_STUB_HANDLE_DECLAER(name) = UT_FUNC_ENTRY_STUB_INIT(oldfunc, newfunc)

#define UT_RETURN_STUB_DEFINE(name, func, rettype, retvalue)		\
	UT_STUB_AGENT_DEFINE(name, rettype, retvalue) \
	UT_ENTRY_STUB_HANDLE_DECLAER(name) = UT_FUNC_RETUEN_STUB_INIT_WITH_AGENT(func, \
						_UT_STUB_AGENT_ENTRY(name), \
						&(_UT_STUB_AGENT_RETURN(name)))

#define ut_stub_active_entry(handle, func) ({ \
	struct ut_stub_data stub_data = UT_ENTRY_STUB_INIT(func,func); \
	_ut_stub_register(handle, &stub_data, __FILE__, __LINE__);\
})

#define ut_stub_active_return(handle, value) ({ \
	struct ut_stub_data stub_data = UT_RETUEN_STUB_INIT(value,value); \
	_ut_stub_register(handle, &stub_data, __FILE__, __LINE__);\
})

#define ut_stub_deactive_entry(handle) \
	_ut_stub_unregister(handle, __FILE__, __LINE__)

#define ut_stub_deactive_return(handle) \
	_ut_stub_unregister(handle, __FILE__, __LINE__)


#define _UT_ENTRY_STUB_VAR(handler)  __ut_entry_##handler
#define _UT_RETURN_STUB_VAR(handler) __ut_return_##handler

/**
 * UT_ENTRY_STUB_DECLARE() - declare entry stub handle of funciton @stub_handler
 */
#define UT_ENTRY_STUB_DECLARE(stub_handler)		\
	UT_ENTRY_STUB_HANDLE_DECLAER(_UT_ENTRY_STUB_VAR(stub_handler))

/**
 * UT_RETURN_STUB_DECLARE() - declare return stub handle of funciton @stub_handler
 */
#define UT_RETURN_STUB_DECLARE(stub_handler)		\
	UT_ENTRY_STUB_HANDLE_DECLAER(_UT_RETURN_STUB_VAR(stub_handler))

/**
 * UT_ENTRY_STUB() - define entry stub to funcion @stub_handler
 */
#define UT_ENTRY_STUB(stub_handler, newfunc)		\
	UT_ENTRY_STUB_DEFINE(_UT_ENTRY_STUB_VAR(stub_handler),stub_handler,newfunc)

/**
 * UT_RETURN_STUB() - define return stub to funcion @stub_handler 
 */
#define UT_RETURN_STUB_WITH_TYPE(stub_handler, rettype, retvalue)		\
	UT_RETURN_STUB_DEFINE(_UT_RETURN_STUB_VAR(stub_handler),stub_handler,rettype,retvalue)

#define UT_RETURN_STUB(stub_handler, retvalue)		\
	UT_RETURN_STUB_WITH_TYPE(stub_handler, ut_stub_ret_t, retvalue)

/**
 * UT_REGISTER_ENTRY_STUB() - fail if the entry of @func cannot replace @stub_handler
 * @stub_handler: address expression of old entry
 * @func: address expression of new entry
 */
#define UT_REGISTER_ENTRY_STUB(stub_handler, func) \
	ut_stub_active_entry(&(_UT_ENTRY_STUB_VAR(stub_handler)), func)

/**
 * UT_REGISTER_RETURN_STUB() - fail if the return value of @func cannot replace @value
 * @stub_handler: address expression of entry
 * @value: new return value expression of entry
 */
#define UT_REGISTER_RETURN_STUB(stub_handler, value) \
	ut_stub_active_return(&(_UT_RETURN_STUB_VAR(stub_handler)),value)

/**
 * UT_UNREGISTER_ENTRY_STUB() - restore the old entry of @stub_handler
 * @stub_handler: address expression of old entry
 */
#define UT_UNREGISTER_ENTRY_STUB(stub_handler) \
	ut_stub_deactive_entry(&(_UT_ENTRY_STUB_VAR(stub_handler)))

/**
 * UT_UNREGISTER_ENTRY_STUB() - restore the old return vaule of @stub_handler
 * @stub_handler: address expression of old entry
 */
#define UT_UNREGISTER_RETURN_STUB(stub_handler) \
	ut_stub_deactive_return(&(_UT_RETURN_STUB_VAR(stub_handler)))

int ut_stub_clear_all();

/* assert, expect macro define */

/* unit test check flag */
#define _UT_CK_FAIL_NOP			    (0)
#define _UT_CK_FAIL_EXIT			(1)

#define _UT_CK_TYPE_EXPECT			(0)
#define _UT_CK_TYPE_ASSERT			(1)
#define _UT_CK_TYPE_TEST			(2)
#define _UT_CK_TYPE_STUB_ACTIVE		(3)
#define _UT_CK_TYPE_STUB_DEACTIVE	(4)
#define _UT_CK_TYPE_STUB_UPDATE		(5)

#define UT_CK_FLAG(fail_op, type)	((type & 0xFF) | ((fail_op)<<8))
#define UT_CK_TYPE(flag)  			(flag & 0xFF)
#define UT_CK_OP(flag)  			(((flag)>>8) & 0xFF)


int _ut_fail_unless (int cond, int flag, const char *file,
			int line, const char *fmt, ...);



char *_ut_mem_to_str(void *d, int size, char *s, int len);


/* Fail the test case unless expr is true */
/* The space before the comma sign before ## is essential to be compatible
   with gcc 2.95.3 and earlier.
*/
#define ut_fail_unless_msg(expr, check_type, format, ...)			\
			_ut_fail_unless(expr, check_type, __FILE__, __LINE__,	\
					format, ## __VA_ARGS__)

#define ut_fail_unless(expr, check_type,...)\
	        _ut_fail_unless(expr, check_type, __FILE__, __LINE__,		\
					"", ## __VA_ARGS__)

#define _ut_check_prefix "Expected: "
#define _ut_check_word_sep " vs "
#define _ut_check_line_sep "\n"
#define _ut_check_block_sep "\n---\n"
/* ==== basic check macro define === */
#define _ut_check_true(ck_type, X, fmt,...) ({ \
		int x = (X); \
		ut_fail_unless_msg(x, ck_type, \
			_ut_check_prefix "'%s == true' : 0x%x " fmt, \
			#X, x, ## __VA_ARGS__); \
	})

#define _ut_check_false(ck_type, X, fmt,...) ({ \
		int x = (X); \
		ut_fail_unless_msg(!x, ck_type, \
			_ut_check_prefix "'%s == false' : 0x%x " fmt, \
			#X, x, ## __VA_ARGS__); \
	})
	
/* Integer comparsion macros with improved output compared to fail_unless(). */
/* O may be any comparion operator. */
#define _ut_check_int(ck_type, X, O, Y, fmt,...) ({ \
		int x = (X); int y = (Y); \
		ut_fail_unless_msg(x O y, ck_type, \
			_ut_check_prefix "'%s "#O" %s' : 0x%x" _ut_check_word_sep "0x%x " fmt, \
			#X, #Y, x, y, ## __VA_ARGS__); \
	})

#define _ut_check_long(ck_type, X, O, Y, fmt,...) ({ \
		long x = (X); long y = (Y); \
		ut_fail_unless_msg(x O y, ck_type, \
			_ut_check_prefix "'%s "#O" %s' : 0x%lx" _ut_check_word_sep "0x%lx " fmt, \
			#X, #Y, x, y, ## __VA_ARGS__); \
	})
	

/* String comparsion macros with improved output compared to fail_unless() */
#define _ut_check_str_eq(ck_type, X, Y, fmt,...) ({ \
		const char* x = (X); const char* y = (Y); \
		int cmpret = (strcmp(x,y) == 0); \
		ut_fail_unless_msg(cmpret, ck_type, \
			_ut_check_prefix "'%s == %s' :" \
			_ut_check_line_sep "(%p)\"%s\"" \
			_ut_check_line_sep "(%p)\"%s\"" \
			_ut_check_line_sep fmt, \
			#X, #Y, x, x, y, y, ## __VA_ARGS__);\
	})

#define _ut_check_str_ne(ck_type, X, Y, fmt,...) ({ \
		const char* x = (X); const char* y = (Y);\
		int cmpret = (strcmp(x,y) != 0); \
		ut_fail_unless_msg(cmpret, ck_type, \
			_ut_check_prefix "'%s != %s' :" \
			_ut_check_line_sep "(%p/%p)\"%s\"" \
			_ut_check_line_sep fmt,\
			#X, #Y, x, y, x, ## __VA_ARGS__);\
	})

/* Memory comparsion macros with improved output compared to fail_unless() */
#define _ut_check_mem_eq(ck_type, X, Y, size, fmt,...) ({ \
		const void* x = (X); const void* y = (Y);int s = (size); \
		char x_str[256]; char y_str[256]; \
		ut_fail_unless_msg(memcmp(x,y,s) == 0, ck_type, \
			_ut_check_prefix "'%s == %s' :" \
			_ut_check_block_sep "(%p)\n%s" \
			_ut_check_block_sep "(%p)\n%s" \
			_ut_check_block_sep fmt, \
			#X, #Y, x, _ut_mem_to_str(x,s,x_str,sizeof(x_str)), \
			y, _ut_mem_to_str(y,s,y_str,sizeof(y_str)), \
			## __VA_ARGS__);\
	})

#define _ut_check_mem_ne(ck_type, X, Y, size, fmt,...) ({ \
		const void* x = (X); const void* y = (Y);int s = (size); \
		char x_str[256]; char y_str[256]; \
		ut_fail_unless_msg(memcmp(x,y,s) != 0, ck_type, \
			_ut_check_prefix "'%s != %s' :" \
			_ut_check_block_sep "(%p/%p)\n%s" \
			_ut_check_block_sep fmt, \
			#X, #Y, x, y, _ut_mem_to_str(x,s,x_str,sizeof(x_str)), \
			## __VA_ARGS__);\
	})


#define UT_CK_FLAG_ASSERT  UT_CK_FLAG(_UT_CK_FAIL_EXIT, _UT_CK_TYPE_ASSERT)
#define UT_CK_FLAG_EXPECT  UT_CK_FLAG(_UT_CK_FAIL_NOP, _UT_CK_TYPE_EXPECT)
#define UT_CK_FLAG_TEST   UT_CK_FLAG(_UT_CK_FAIL_NOP, _UT_CK_TYPE_TEST)

/**
 * UT_TEST_TRUE() - fail and return if @C evaluates to false
 * @C: Boolean expression to evaluate
 */
#define UT_TEST_TRUE(C) \
	_ut_check_true(UT_CK_FLAG_TEST, (C),"")

#define UT_TEST_TRUE_MSG(C, fmt,...) \
	_ut_check_true(UT_CK_FLAG_TEST, (C), fmt, ##__VA_ARGS__)

/**
 * UT_TEST_FALSE() - fail and return if @C evaluates to true
 * @C: Boolean expression to evaluate
 */
#define UT_TEST_FALSE(C) \
	_ut_check_false(UT_CK_FLAG_TEST, (C),"")
#define UT_TEST_FALSE_MSG(C, fmt,...) \
	_ut_check_false(UT_CK_FLAG_TEST, (C), fmt, ##__VA_ARGS__)

/**
 * UT_TEST_LONG_EQ() - compare two longs, fail and return if @X != @Y
 * @X: Expected value
 * @Y: Actual value
 */
#define UT_TEST_LONG_EQ(X, Y)	\
	_ut_check_long(UT_CK_FLAG_TEST, X, ==, Y,"")
#define UT_TEST_LONG_EQ_MSG(X, Y, fmt,...) \
	_ut_check_long(UT_CK_FLAG_TEST, X, ==, Y, fmt, ##__VA_ARGS__)
#define UT_TEST_LONG_NE(X, Y)	\
		_ut_check_long(UT_CK_FLAG_TEST, X, !=, Y,"")
#define UT_TEST_LONG_NE_MSG(X, Y, fmt,...) \
		_ut_check_long(UT_CK_FLAG_TEST, X, !=, Y, fmt, ##__VA_ARGS__)
#define UT_TEST_LONG_GE(X, Y)	\
		_ut_check_long(UT_CK_FLAG_TEST, X, >=, Y,"")
#define UT_TEST_LONG_GE_MSG(X, Y, fmt,...) \
		_ut_check_long(UT_CK_FLAG_TEST, X, >=, Y, fmt, ##__VA_ARGS__)
#define UT_TEST_ADDR_EQ(X, Y) \
	_ut_check_long(UT_CK_FLAG_TEST, (u64)(X), ==, (u64)(Y),"")
#define UT_TEST_ADDR_EQ_MSG(X, Y, fmt,...) \
	_ut_check_long(UT_CK_FLAG_TEST, (u64)(X), ==, (u64)(Y), fmt, ##__VA_ARGS__)
#define UT_TEST_ADDR_NE(X, Y) \
		_ut_check_long(UT_CK_FLAG_TEST, (u64)(X), !=, (u64)(Y),"")
#define UT_TEST_ADDR_NE_MSG(X, Y, fmt,...) \
		_ut_check_long(UT_CK_FLAG_TEST, (u64)(X), !=, (u64)(Y), fmt, ##__VA_ARGS__)
#define UT_TEST_ADDR_GE(X, Y) \
		_ut_check_long(UT_CK_FLAG_TEST, (u64)(X), >=, (u64)(Y),"")
#define UT_TEST_ADDR_GE_MSG(X, Y, fmt,...) \
		_ut_check_long(UT_CK_FLAG_TEST, (u64)(X), >=, (u64)(Y), fmt, ##__VA_ARGS__)
#define UT_TEST_INT_EQ(X, Y) \
	_ut_check_int(UT_CK_FLAG_TEST, X, ==, Y,"")
#define UT_TEST_INT_EQ_MSG(X, Y, fmt,...) \
	_ut_check_int(UT_CK_FLAG_TEST, X, ==, Y, fmt, ##__VA_ARGS__)
#define UT_TEST_INT_GT(X, Y) \
	_ut_check_int(UT_CK_FLAG_TEST, X, >, Y,"")
#define UT_TEST_INT_GT_MSG(X, Y, fmt,...)	\
	_ut_check_int(UT_CK_FLAG_TEST, X, >, Y, fmt, ##__VA_ARGS__)
#define UT_TEST_INT_NE(X, Y) \
	_ut_check_int(UT_CK_FLAG_TEST, X, !=, Y,"")
#define UT_TEST_INT_NE_MSG(X, Y, fmt,...) \
	_ut_check_int(UT_CK_FLAG_TEST, X, !=, Y, fmt, ##__VA_ARGS__)
#define UT_TEST_INT_GE(X, Y) \
		_ut_check_int(UT_CK_FLAG_TEST, X, >=, Y,"")
#define UT_TEST_INT_GE_MSG(X, Y, fmt,...) \
		_ut_check_int(UT_CK_FLAG_TEST, X, >=, Y, fmt, ##__VA_ARGS__)


/**
 * UT_ASSERT_TRUE() - fail and return if @C evaluates to false
 * @C: Boolean expression to evaluate
 */
#define UT_ASSERT_TRUE(C) \
	_ut_check_true(UT_CK_FLAG_ASSERT, (C),"")

#define UT_ASSERT_TRUE_MSG(C, fmt,...) \
	_ut_check_true(UT_CK_FLAG_ASSERT, (C), fmt, ##__VA_ARGS__)


/**
 * UT_ASSERT_FALSE() - fail and return if @C evaluates to true
 * @C: Boolean expression to evaluate
 */
#define UT_ASSERT_FALSE(C) \
	_ut_check_false(UT_CK_FLAG_ASSERT, (C),"")
#define UT_ASSERT_FALSE_MSG(C, fmt,...) \
	_ut_check_false(UT_CK_FLAG_ASSERT, (C), fmt, ##__VA_ARGS__)

/**
 * UT_ASSERT_LONG_EQ() - compare two longs, fail and return if @X != @Y
 * @X: Expected value
 * @Y: Actual value
 */
#define UT_ASSERT_LONG_EQ(X, Y)	\
	_ut_check_long(UT_CK_FLAG_ASSERT, X, ==, Y,"")
#define UT_ASSERT_LONG_EQ_MSG(X, Y, fmt,...) \
	_ut_check_long(UT_CK_FLAG_ASSERT, X, ==, Y, fmt, ##__VA_ARGS__)
#define UT_ASSERT_LONG_NE(X, Y)	\
		_ut_check_long(UT_CK_FLAG_ASSERT, X, !=, Y,"")
#define UT_ASSERT_LONG_NE_MSG(X, Y, fmt,...) \
		_ut_check_long(UT_CK_FLAG_ASSERT, X, !=, Y, fmt, ##__VA_ARGS__)
#define UT_ASSERT_LONG_GE(X, Y)	\
		_ut_check_long(UT_CK_FLAG_ASSERT, X, >=, Y,"")
#define UT_ASSERT_LONG_GE_MSG(X, Y, fmt,...) \
		_ut_check_long(UT_CK_FLAG_ASSERT, X, >=, Y, fmt, ##__VA_ARGS__)
#define UT_ASSERT_ADDR_EQ(X, Y) \
	_ut_check_long(UT_CK_FLAG_ASSERT, (u64)(X), ==, (u64)(Y),"")
#define UT_ASSERT_ADDR_EQ_MSG(X, Y, fmt,...) \
	_ut_check_long(UT_CK_FLAG_ASSERT, (u64)(X), ==, (u64)(Y), fmt, ##__VA_ARGS__)
#define UT_ASSERT_ADDR_NE(X, Y) \
		_ut_check_long(UT_CK_FLAG_ASSERT, (u64)(X), !=, (u64)(Y),"")
#define UT_ASSERT_ADDR_NE_MSG(X, Y, fmt,...) \
		_ut_check_long(UT_CK_FLAG_ASSERT, (u64)(X), !=, (u64)(Y), fmt, ##__VA_ARGS__)
#define UT_ASSERT_ADDR_GE(X, Y) \
		_ut_check_long(UT_CK_FLAG_ASSERT, (u64)(X), >=, (u64)(Y),"")
#define UT_ASSERT_ADDR_GE_MSG(X, Y, fmt,...) \
		_ut_check_long(UT_CK_FLAG_ASSERT, (u64)(X), >=, (u64)(Y), fmt, ##__VA_ARGS__)
#define UT_ASSERT_INT_EQ(X, Y) \
	_ut_check_int(UT_CK_FLAG_ASSERT, X, ==, Y,"")
#define UT_ASSERT_INT_EQ_MSG(X, Y, fmt,...) \
	_ut_check_int(UT_CK_FLAG_ASSERT, X, ==, Y, fmt, ##__VA_ARGS__)
#define UT_ASSERT_INT_GT(X, Y) \
	_ut_check_int(UT_CK_FLAG_ASSERT, X, >, Y,"")
#define UT_ASSERT_INT_GT_MSG(X, Y, fmt,...)	\
	_ut_check_int(UT_CK_FLAG_ASSERT, X, >, Y, fmt, ##__VA_ARGS__)
#define UT_ASSERT_INT_NE(X, Y) \
	_ut_check_int(UT_CK_FLAG_ASSERT, X, !=, Y,"")
#define UT_ASSERT_INT_NE_MSG(X, Y, fmt,...) \
	_ut_check_int(UT_CK_FLAG_ASSERT, X, !=, Y, fmt, ##__VA_ARGS__)
#define UT_ASSERT_INT_GE(X, Y) \
		_ut_check_int(UT_CK_FLAG_ASSERT, X, >=, Y,"")
#define UT_ASSERT_INT_GE_MSG(X, Y, fmt,...) \
		_ut_check_int(UT_CK_FLAG_ASSERT, X, >=, Y, fmt, ##__VA_ARGS__)

#define UT_ASSERT_INT_LT(X, Y) \
		_ut_check_int(UT_CK_FLAG_ASSERT, X, <, Y,"")
#define UT_ASSERT_INT_LT_MSG(X, Y, fmt,...) \
		_ut_check_int(UT_CK_FLAG_ASSERT, X, >, Y, fmt, ##__VA_ARGS__)

#define UT_ASSERT_INT_LE(X, Y) \
			_ut_check_int(UT_CK_FLAG_ASSERT, X, <=, Y,"")
#define UT_ASSERT_INT_LE_MSG(X,Y, fmt,...) \
			_ut_check_int(UT_CK_FLAG_ASSERT, X, <=, Y, fmt, ##__VA_ARGS__)

#define UT_ASSERT_STREQ(X, Y) \
	_ut_check_str_eq(UT_CK_FLAG_ASSERT, X, Y,"")
#define UT_ASSERT_STREQ_MSG(X, Y, fmt,...) \
	_ut_check_str_eq(UT_CK_FLAG_ASSERT, X, Y, fmt, ##__VA_ARGS__)
#define UT_ASSERT_STRNE(X, Y) \
	_ut_check_str_ne(UT_CK_FLAG_ASSERT, X, Y,"")
#define UT_ASSERT_STRNE_MSG(X, Y, fmt,...) \
	_ut_check_str_ne(UT_CK_FLAG_ASSERT, X, Y, fmt, ##__VA_ARGS__)


#define UT_ASSERT_MEMEQ(X, Y, S) \
	_ut_check_mem_eq(UT_CK_FLAG_ASSERT, X, Y, S,"")
#define UT_ASSERT_MEMEQ_MSG(X, Y, S, fmt,...) \
	_ut_check_mem_eq(UT_CK_FLAG_ASSERT, X, Y, S, fmt, ##__VA_ARGS__)
#define UT_ASSERT_MEMNE(X, Y, S) \
	_ut_check_mem_ne(UT_CK_FLAG_ASSERT, X, Y, S,"")
#define UT_ASSERT_MEMNE_MSG(X, Y, S, fmt,...) \
	_ut_check_mem_ne(UT_CK_FLAG_ASSERT, X, Y, S, fmt, ##__VA_ARGS__)

/**
 * UT_EXPECT_TRUE() - fail and return if @C evaluates to false
 * @C: Boolean expression to evaluate
 *
 */
#define UT_EXPECT_TRUE(C) \
	_ut_check_true(UT_CK_FLAG_EXPECT, (C),"")
#define UT_EXPECT_TRUE_MSG(C, fmt,...) \
	_ut_check_true(UT_CK_FLAG_EXPECT, (C), fmt, ##__VA_ARGS__)

/**
 * UT_EXPECT_FALSE() - fail and return if @C evaluates to true
 * @C: Boolean expression to evaluate
 */
#define UT_EXPECT_FALSE(C) \
	_ut_check_false(UT_CK_FLAG_EXPECT, (C),"")
#define UT_EXPECT_FALSE_MSG(C, fmt,...) \
	_ut_check_false(UT_CK_FLAG_EXPECT, (C), fmt, ##__VA_ARGS__)

/**
 * UT_EXPECT_LONG_EQ() - compare two longs, fail and return if @X != @Y
 * @X: Expected value
 * @Y: Actual value
 */
#define UT_EXPECT_LONG_EQ(X, Y) \
	_ut_check_long(UT_CK_FLAG_EXPECT, X, ==, Y,"")
#define UT_EXPECT_LONG_EQ_MSG(X, Y, fmt,...) \
	_ut_check_long(UT_CK_FLAG_EXPECT, X, ==, Y, fmt, ##__VA_ARGS__)
#define UT_EXPECT_LONG_NE(X, Y)	\
		_ut_check_long(UT_CK_FLAG_EXPECT, X, !=, Y,"")
#define UT_EXPECT_LONG_NE_MSG(X, Y, fmt,...) \
		_ut_check_long(UT_CK_FLAG_EXPECT, X, !=, Y, fmt, ##__VA_ARGS__)
#define UT_EXPECT_LONG_GE(X, Y)	\
		_ut_check_long(UT_CK_FLAG_EXPECT, X, >=, Y,"")
#define UT_EXPECT_LONG_GE_MSG(X, Y, fmt,...) \
		_ut_check_long(UT_CK_FLAG_EXPECT, X, >=, Y, fmt, ##__VA_ARGS__)
#define UT_EXPECT_ADDR_EQ(X, Y)	\
	_ut_check_long(UT_CK_FLAG_EXPECT, (u64)(X), ==, (u64)(Y),"")
#define UT_EXPECT_ADDR_EQ_MSG(X, Y, fmt,...) \
	_ut_check_long(UT_CK_FLAG_EXPECT, (u64)(X), ==, (u64)(Y), fmt, ##__VA_ARGS__)
#define UT_EXPECT_ADDR_NE(X, Y)	\
		_ut_check_long(UT_CK_FLAG_EXPECT, (u64)(X), !=, (u64)(Y),"")
#define UT_EXPECT_ADDR_NE_MSG(X, Y, fmt,...) \
		_ut_check_long(UT_CK_FLAG_EXPECT, (u64)(X), !=, (u64)(Y), fmt, ##__VA_ARGS__)
#define UT_EXPECT_ADDR_GE(X, Y)	\
		_ut_check_long(UT_CK_FLAG_EXPECT, (u64)(X), >=, (u64)(Y),"")
#define UT_EXPECT_ADDR_GE_MSG(X, Y, fmt,...) \
		_ut_check_long(UT_CK_FLAG_EXPECT, (u64)(X), >=, (u64)(Y), fmt, ##__VA_ARGS__)
#define UT_EXPECT_INT_EQ(X, Y) \
	_ut_check_int(UT_CK_FLAG_EXPECT, X, ==, Y,"")
#define UT_EXPECT_INT_EQ_MSG(X, Y, fmt,...) \
	_ut_check_int(UT_CK_FLAG_EXPECT, X, ==, Y, fmt, ##__VA_ARGS__)
#define UT_EXPECT_INT_GT(X, Y) \
		_ut_check_int(UT_CK_FLAG_EXPECT, X, >, Y,"")
#define UT_EXPECT_INT_GT_MSG(X, Y, fmt,...) \
		_ut_check_int(UT_CK_FLAG_EXPECT, X, >, Y, fmt, ##__VA_ARGS__)
#define UT_EXPECT_INT_LT(X, Y) \
		_ut_check_int(UT_CK_FLAG_EXPECT, X, <, Y,"")
#define UT_EXPECT_INT_LT_MSG(X, Y, fmt,...) \
		_ut_check_int(UT_CK_FLAG_EXPECT, X, <, Y, fmt, ##__VA_ARGS__)
#define UT_EXPECT_INT_NE(X, Y) \
		_ut_check_int(UT_CK_FLAG_EXPECT, X, !=, Y,"")
#define UT_EXPECT_INT_NE_MSG(X,Y, fmt,...) \
		_ut_check_int(UT_CK_FLAG_EXPECT, X, !=, Y, fmt, ##__VA_ARGS__)
#define UT_EXPECT_INT_GE(X, Y) \
		_ut_check_int(UT_CK_FLAG_EXPECT, X, >=, Y,"")
#define UT_EXPECT_INT_GE_MSG(X,Y, fmt,...) \
		_ut_check_int(UT_CK_FLAG_EXPECT, X, >=, Y, fmt, ##__VA_ARGS__)
#define UT_EXPECT_INT_LE(X, Y) \
			_ut_check_int(UT_CK_FLAG_EXPECT, X, <=, Y,"")
#define UT_EXPECT_INT_LE_MSG(X,Y, fmt,...) \
			_ut_check_int(UT_CK_FLAG_EXPECT, X, <=, Y, fmt, ##__VA_ARGS__)


#define UT_EXPECT_STR_EQ(X, Y) \
	_ut_check_str_eq(UT_CK_FLAG_EXPECT, X, Y,"")
#define UT_EXPECT_STR_EQ_MSG(X, Y, fmt,...) \
	_ut_check_str_eq(UT_CK_FLAG_EXPECT, X, Y, fmt, ##__VA_ARGS__)
#define UT_EXPECT_STR_NE(X, Y) \
	_ut_check_str_ne(UT_CK_FLAG_EXPECT, X, Y,"")
#define UT_EXPECT_STR_NE_MSG(X, Y, fmt,...) \
	_ut_check_str_ne(UT_CK_FLAG_EXPECT, X, Y, fmt, ##__VA_ARGS__)

#define UT_EXPECT_MEM_EQ(X, Y, S) \
	_ut_check_mem_eq(UT_CK_FLAG_EXPECT, X, Y, S,"")
#define UT_EXPECT_MEM_EQ_MSG(X, Y, S, fmt,...) \
	_ut_check_mem_eq(UT_CK_FLAG_EXPECT, X, Y, S, fmt, ##__VA_ARGS__)
#define UT_EXPECT_MEM_NE(X, Y, S) \
	_ut_check_mem_ne(UT_CK_FLAG_EXPECT, X, Y, S,"")
#define UT_EXPECT_MEM_NE_MSG(X, Y, S, fmt,...) \
	_ut_check_mem_ne(UT_CK_FLAG_EXPECT, X, Y, S, fmt, ##__VA_ARGS__)

#define UT_EXPECT_STREQ(X, Y) UT_EXPECT_STR_EQ(X, Y)
#define UT_EXPECT_STREQ_MSG(X, Y, fmt,...) UT_EXPECT_STR_EQ_MSG(X, Y, fmt,##__VA_ARGS__)
#define UT_EXPECT_STRNE(X, Y) UT_EXPECT_STR_NE(X, Y)
#define UT_EXPECT_STRNE_MSG(X, Y, fmt,...) UT_EXPECT_STR_NE_MSG(X, Y, fmt,##__VA_ARGS__)

#define UT_EXPECT_MEMEQ(X, Y, S) UT_EXPECT_MEM_EQ(X, Y, S)
#define UT_EXPECT_MEMEQ_MSG(X, Y, S, fmt,...) UT_EXPECT_MEM_EQ_MSG(X, Y, S, fmt,##__VA_ARGS__)
#define UT_EXPECT_MEMNE(X, Y, S) UT_EXPECT_MEM_NE(X, Y, S)
#define UT_EXPECT_MEMNE_MSG(X, Y, S, fmt,...) UT_EXPECT_MEM_NE_MSG(X, Y, S, fmt,##__VA_ARGS__)

/* define for common */
#define ASSERT_TRUE(C) 				UT_ASSERT_TRUE(C)
#define ASSERT_INT_EQ(orig, value)		UT_ASSERT_INT_EQ(orig, value)
#define ASSERT_INT_NE(orig, value)		UT_ASSERT_INT_NE(orig, value)
#define ASSERT_INT_LT(orig, value)		UT_ASSERT_INT_LT(orig, value)
#define ASSERT_INT_LE(orig, value)		UT_ASSERT_INT_LE(orig, value)
#define ASSERT_INT_GT(orig, value)		UT_ASSERT_INT_GT(orig, value)
#define ASSERT_INT_GE(orig, value)		UT_ASSERT_INT_GE(orig, value)
#define ASSERT_LONG_EQ(orig, value)		UT_ASSERT_LONG_EQ(orig, value)
#define ASSERT_LONG_NE(orig, value)		UT_ASSERT_LONG_NE(orig, value)
#define ASSERT_STR_EQ(orig, value)		UT_ASSERT_STREQ(orig, value)
#define ASSERT_STR_NE(orig, value)		UT_ASSERT_STRNE(orig, value)
#define ASSERT_MEM_EQ(orig, value, size)	UT_ASSERT_MEMEQ(orig, value, size)
#define ASSERT_MEM_NE(orig, value, size)	UT_ASSERT_MEMNE(orig, value, size)
#define ASSERT_ADDR_EQ(orig, value)		UT_ASSERT_ADDR_EQ(orig, value)
#define ASSERT_ADDR_NE(orig, value)		UT_ASSERT_ADDR_NE(orig, value)

#define EXPECT_INT_EQ(orig, value)		UT_EXPECT_INT_EQ(orig, value)
#define EXPECT_INT_NE(orig, value)		UT_EXPECT_INT_NE(orig, value)
#define EXPECT_INT_LT(orig, value)		UT_EXPECT_INT_LT(orig, value)
#define EXPECT_INT_LE(orig, value)		UT_EXPECT_INT_LE(orig, value)
#define EXPECT_INT_GT(orig, value)		UT_EXPECT_INT_GT(orig, value)
#define EXPECT_INT_GE(orig, value)		UT_EXPECT_INT_GE(orig, value)
#define EXPECT_LONG_EQ(orig, value)		UT_EXPECT_LONG_EQ(orig, value)
#define EXPECT_LONG_NE(orig, value)		UT_EXPECT_LONG_NE(orig, value)
#define EXPECT_STR_EQ(orig, value)		UT_EXPECT_STR_EQ(orig, value)
#define EXPECT_STR_NE(orig, value)		UT_EXPECT_STR_NE(orig, value)
#define EXPECT_MEM_EQ(orig, value, size)	UT_EXPECT_MEM_EQ(orig, value, size)
#define EXPECT_MEM_NE(orig, value, size)	UT_EXPECT_MEM_NE(orig, value, size)
#define EXPECT_ADDR_EQ(orig, value)		UT_EXPECT_ADDR_EQ(orig, value)
#define EXPECT_ADDR_NE(orig, value)		UT_EXPECT_ADDR_NE(orig, value)


#include "ut_dispatch.h"
#endif
