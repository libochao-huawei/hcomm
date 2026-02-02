#ifndef __LLT_COMMON_H__
#define __LLT_COMMON_H__

/* unit test common */
#define var_zero(a) memset(&a, 0,  sizeof(a))

#define __llt_stringify(...)	#__VA_ARGS__
#define llt_stringify(...)	__llt_stringify(__VA_ARGS__)""

#define __llt_concat(a, b) a ## b
#define llt_concat(a, b) __llt_concat(a, b)

/* unit test list */
struct llt_list_head {
	struct llt_list_head *next, *prev;
};

#define ST_LIST_HEAD_INIT(name) { &(name), &(name) }

static inline void llt_init_list_head(struct llt_list_head *list)
{
	list->next = list;
	list->prev = list;
}

static inline  void llt_list_add(struct llt_list_head *new_list, struct llt_list_head *head)
{
	head->next->prev = new_list;
	new_list->next = head->next;
	new_list->prev = head;
	head->next = new_list;
}

static inline void llt_list_del(struct llt_list_head *entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	entry->next = 0;
	entry->prev = 0;
}

#define llt_list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

int llt_printf (const char *fmt, ...);
static inline char *llt_get_filename(char *filePath, int level)
{
	int len;
	int pos;
	int sep_pos;
	int layer;
	char c;

	if (!filePath)
		return "??";

	len = strlen(filePath);
	if (len > 0)
		pos = len - 1;
	else
		pos = 0;

	layer = 0;
	sep_pos = 0;
	while(pos > 0) {
		c = filePath[pos];
		if (('/' == c) || ('\\' == c)) {
			sep_pos = pos + 1;
			if (layer >= level)
				return filePath + sep_pos;
			else
				layer++;
		}
		pos--;
	}
	return filePath + sep_pos;
}

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
struct llt_map {
	struct llt_list_head entrys;
	char *name;
	int (*match_func)(const void *, const void *);
};

struct llt_map_entry {
	int  id;
	void *key;
	void *data;
};
int llt_map_clear(struct llt_map *m, char *file, int line);
struct llt_map_entry *llt_map_entry_insert(struct llt_map *m, const struct llt_map_entry *item, char * file, int line);
struct llt_map_entry *llt_map_entry_erase(struct llt_map *m, struct llt_map_entry *item, char *file, int line);
struct llt_map_entry *llt_map_entry_find(struct llt_map *m, struct llt_map_entry *item, void **v, char *file, int line);

/* init key-value pairs entry */
#define LLT_ENTRY_INIT(n, k, v) { \
		.id		= (int)(n), \
		.key		= (void *)(k), \
		.data		= (void *)(v)}

#define LLT_MAP_VAR(func,cls) __llt_map_##func##_##cls
#define LLT_MAP_DECLARE(func,cls) \
	struct llt_map LLT_MAP_VAR(func,cls)
	
#define LLT_MAP_DEFINE(func,cls) LLT_MAP_DECLARE(func,cls) = { \
	.name   = __llt_stringify(func) "(" __llt_stringify(cls) ")", \
	.match_func  = NULL, \
	.entrys = ST_LIST_HEAD_INIT(LLT_MAP_VAR(func,cls).entrys) }

#define LLT_MAP_EXT_DEFINE(func,cls,match_fn) LLT_MAP_DECLARE(func,cls) = { \
	.name   = __llt_stringify(func) "(" __llt_stringify(cls) ")", \
	.match_func  = match_fn, \
	.entrys = ST_LIST_HEAD_INIT(LLT_MAP_VAR(func,cls).entrys) }

#define LLT_MAP_CLR(func,cls) llt_map_clear(&LLT_MAP_VAR(func,cls), __FILE__, __LINE__)

#define LLT_MAP_INSERT_WITH_ID(func,cls,k,v,n) ({ \
	struct llt_map_entry e = LLT_ENTRY_INIT(n, k, v); \
	llt_map_entry_insert(&LLT_MAP_VAR(func,cls), &e, __FILE__, __LINE__);})
	
#define LLT_MAP_ERASE_WITH_ID(func,cls,k,n) ({ \
	struct llt_map_entry e = LLT_ENTRY_INIT(n, k, NULL); \
	llt_map_entry_erase(&LLT_MAP_VAR(func,cls), &e, __FILE__, __LINE__);})

#define LLT_MAP_REPLACE_WITH_ID(func,cls,k,v,n) ({ \
	LLT_MAP_ERASE_WITH_ID(func,cls,k,n); \
	LLT_MAP_INSERT_WITH_ID(func,cls,k,v,n);})

#define LLT_MAP_FIND_WITH_ID(func,cls,k,v,n) ({ \
	struct llt_map_entry e = LLT_ENTRY_INIT(n, k, NULL); \
	llt_map_entry_find(&LLT_MAP_VAR(func,cls), &e, (void**)v, __FILE__, __LINE__);})

#define LLT_MAP_INSERT(func,cls,k,v) LLT_MAP_INSERT_WITH_ID(func,cls,k,v,-1)
#define LLT_MAP_ERASE(func,cls,k)    LLT_MAP_ERASE_WITH_ID(func,cls,k,-1)
#define LLT_MAP_REPLACE(func,cls,k,v)  LLT_MAP_REPLACE_WITH_ID(func,cls,k,v,-1)
#define LLT_MAP_EXIST(func,cls,k)    LLT_MAP_FIND_WITH_ID(func,cls,k,NULL,-1)
#define LLT_MAP_FIND(func,cls,k) ({void *__v = NULL; LLT_MAP_FIND_WITH_ID(func,cls,k,&__v,-1) ? __v : NULL;})
#define LLT_MAP_MATCH(func,cls,k,v)    LLT_MAP_FIND_WITH_ID(func,cls,k,v,-1)

/* unit test counter range */
struct llt_cnt_range {
	int counter;
	char *name;
	struct llt_list_head ranges;
};

void _llt_cnt_range_append(struct llt_cnt_range *range_handle, int start, int end, char *file, int line) ;
void _llt_cnt_range_clear(struct llt_cnt_range *counter_range, char *file, int line) ;
int _llt_cnt_range_check(struct llt_cnt_range *counter_range, int step, int *ptr_count, char *file, int line) ;

#define LLT_CNT_RANGE_VAR(func,cls) __ut_range_##func##_##cls
#define LLT_CNT_RANGE_DECLARE(func, cls) struct llt_cnt_range LLT_CNT_RANGE_VAR(func, cls)

#define LLT_CNT_RANGE_DEFINE(func, cls) LLT_CNT_RANGE_DECLARE(func, cls) = { \
	.counter = 0, \
	.name    = __llt_stringify(func) "(" __llt_stringify(cls) ")", \
	.ranges  = ST_LIST_HEAD_INIT(LLT_CNT_RANGE_VAR(func, cls).ranges) }

#define LLT_CNT_RANGE_CHECK(func, cls, step) \
	_llt_cnt_range_check(&LLT_CNT_RANGE_VAR(func, cls), step, NULL, __FILE__, __LINE__)

#define LLT_CNT_RANGE_CHECK_AND_GET(func, cls, step, ptr_cnt) \
	_llt_cnt_range_check(&LLT_CNT_RANGE_VAR(func, cls), step, ptr_cnt, __FILE__, __LINE__)

#define LLT_CNT_RANGE_ADD(func, cls, start, end) \
	_llt_cnt_range_append(&LLT_CNT_RANGE_VAR(func, cls), start, end, __FILE__, __LINE__)

#define LLT_CNT_RANGE_CLR(func, cls) \
	_llt_cnt_range_clear(&LLT_CNT_RANGE_VAR(func, cls),__FILE__, __LINE__)

/* compose trigger */
#define LLT_CNT_MAP_ADD(func, cls, start, end, val) do { \
	int cnt = 0;	\
	LLT_CNT_RANGE_ADD(func, cls, start, end);\
	for (cnt = start; cnt <= end; cnt++) \
		LLT_MAP_INSERT(func, cls, cnt, val);\
} while(0)

#define LLT_CNT_MAP_CLR(func, cls) do { \
	LLT_CNT_RANGE_CLR(func, cls);\
	LLT_MAP_CLR(func, cls);\
} while(0)

/* unit test malloc */
#define LLT_MEM_FLAG_NOCHECK 0x00000001
#define LLT_MEM_FLAG_ZERO    0x00000002
#define LLT_MEM_FLAG_RANDOM  0x00000004

void *llt_mem_oob_data(void *p);
int llt_mem_size(void *p);
void *llt_malloc_align_trace(int size, int oob_size, int align_shift, int flags,
			     const char *file, int line, const char *fmt, ...);

#define llt_malloc_trace(size, oob_size, flags, file, line, fmt, ...) \
		llt_malloc_align_trace(size, oob_size, 4, flags, file, line, fmt, ##__VA_ARGS__)
void llt_free_trace(void *p, int flags, const char *file, int line, const char *fmt, ...);
int llt_mem_error();
	
#define llt_zalloc(size) llt_malloc_trace(size, 0, LLT_MEM_FLAG_ZERO, __FILE__, __LINE__, 0)
#define llt_malloc(size) llt_malloc_trace(size, 0, 0, __FILE__, __LINE__, 0)
#define llt_free(p) 	llt_free_trace(p, 0, __FILE__, __LINE__, 0)
#define llt_mfree(p, flags) 	llt_free_trace(p, flags, __FILE__, __LINE__, 0)

/* unit test function stub */
typedef	int (*llt_stub_fn_t)(void *data);
typedef	void * llt_stub_ret_t;

struct llt_stub_data {
	int type;
	char *name;
	union {
		llt_stub_fn_t func;
		llt_stub_ret_t ret;
	} val;
};

struct llt_stub_handle {
	llt_stub_fn_t func;
	struct llt_stub_data body;
	struct llt_stub_data init;
	struct llt_stub_data stub;
};

#define LLT_STUB_TYPE_ENTRY   0x50
#define LLT_STUB_TYPE_RETURN  0x51

/* init function entry stub */
#define LLT_ENTRY_STUB_INIT(nm,fn) {		 \
		.type		= LLT_STUB_TYPE_ENTRY, \
		.name		= llt_stringify(nm), \
		.val.func	= (llt_stub_fn_t)(fn) }

/* init function return stub */
#define LLT_RETUEN_STUB_INIT(nm,value) {		 \
		.type		= LLT_STUB_TYPE_RETURN, \
		.name		= llt_stringify(nm), \
		.val.ret	= (llt_stub_ret_t)(value) }


/* init stub for replace function entry to new */
#define LLT_FUNC_ENTRY_STUB_INIT(origfunc, newfunc) { \
		.func	= (llt_stub_fn_t)origfunc, \
		.body	= LLT_ENTRY_STUB_INIT(origfunc, origfunc), \
		.init	= LLT_ENTRY_STUB_INIT(origfunc, origfunc), \
		.stub	= LLT_ENTRY_STUB_INIT(newfunc, newfunc) }

/* init stub for replace function return value to other */
#define LLT_FUNC_RETUEN_STUB_INIT(origfunc, retval) {		 \
		.func	= (llt_stub_fn_t)origfunc,	\
		.body	= LLT_ENTRY_STUB_INIT(origfunc, origfunc), \
		.init	= LLT_RETUEN_STUB_INIT(origfunc, retval), \
		.stub	= LLT_RETUEN_STUB_INIT(retval, retval) }

#define LLT_FUNC_RETUEN_STUB_INIT_WITH_AGENT(name, origfunc, rettype) {		 \
		.func	= (llt_stub_fn_t)origfunc,	\
		.body	= LLT_ENTRY_STUB_INIT(_LLT_STUB_AGENT_ENTRY(name), _LLT_STUB_AGENT_ENTRY(name)), \
		.init	= LLT_RETUEN_STUB_INIT(origfunc, &(_LLT_STUB_AGENT_RETURN(name))), \
		.stub	= LLT_RETUEN_STUB_INIT(origfunc, origfunc) }


/* stub agent define */
#define _LLT_STUB_AGENT_RETURN(handler) _stub_agent##handler##_return
#define _LLT_STUB_AGENT_ENTRY(handler)  _stub_agent##handler##_entry

#define LLT_STUB_AGENT_DEFINE(name, rettype, retvalue) \
	static rettype _LLT_STUB_AGENT_RETURN(name) = (rettype)(retvalue); \
	static rettype _LLT_STUB_AGENT_ENTRY(name)(void *data) { \
		llt_printf("\n<" _CONSOLE_DISP_PURPLE "%s" _CONSOLE_DISP_NONE "> %s:%d\n", __FUNCTION__, llt_get_filename(__FILE__, 1),__LINE__);\
		return _LLT_STUB_AGENT_RETURN(name); \
	}

int _llt_stub_register(struct llt_stub_handle *stub_handle, 
						struct llt_stub_data *newstub, char *f, int line);
void _llt_stub_unregister(struct llt_stub_handle *stub_handle, char * f, int line);

#define LLT_ENTRY_STUB_HANDLE_DECLAER(name)	\
	struct llt_stub_handle name

#define LLT_ENTRY_STUB_DEFINE(name, oldfunc, newfunc)		\
	LLT_ENTRY_STUB_HANDLE_DECLAER(name) = LLT_FUNC_ENTRY_STUB_INIT(oldfunc, newfunc)

#define LLT_RETURN_STUB_DEFINE(name, func, rettype, retvalue)		\
	LLT_STUB_AGENT_DEFINE(name, rettype, retvalue); \
	LLT_ENTRY_STUB_HANDLE_DECLAER(name) = LLT_FUNC_RETUEN_STUB_INIT_WITH_AGENT(name, func, rettype)

#define LLT_RETURN_STUB_DEFINE_LOCAL(name, func, rettype, retvalue)		\
	LLT_STUB_AGENT_DEFINE(name, rettype, retvalue); \
	static LLT_ENTRY_STUB_HANDLE_DECLAER(name) = LLT_FUNC_RETUEN_STUB_INIT_WITH_AGENT(name, func, rettype)


#define llt_stub_active_entry(handle, func) ({ \
	struct llt_stub_data stub_data = LLT_ENTRY_STUB_INIT(func,func); \
	_llt_stub_register(handle, &stub_data, __FILE__, __LINE__);\
})

#define llt_stub_active_return(handle, value) ({ \
	struct llt_stub_data stub_data = LLT_RETUEN_STUB_INIT(value,value); \
	_llt_stub_register(handle, &stub_data, __FILE__, __LINE__);\
})

#define llt_stub_deactive_entry(handle) \
	_llt_stub_unregister(handle, __FILE__, __LINE__)

#define llt_stub_deactive_return(handle) \
	_llt_stub_unregister(handle, __FILE__, __LINE__)


#define _LLT_ENTRY_STUB_VAR(handler)  __llt_entry_##handler
#define _LLT_RETURN_STUB_VAR(handler) __llt_return_##handler

/**
 * LLT_ENTRY_STUB_DECLARE() - declare entry stub handle of funciton @stub_handler
 */
#define LLT_ENTRY_STUB_DECLARE(stub_handler)		\
	LLT_ENTRY_STUB_HANDLE_DECLAER(_LLT_ENTRY_STUB_VAR(stub_handler))

/**
 * LLT_RETURN_STUB_DECLARE() - declare return stub handle of funciton @stub_handler
 */
#define LLT_RETURN_STUB_DECLARE(stub_handler)		\
	LLT_ENTRY_STUB_HANDLE_DECLAER(_LLT_RETURN_STUB_VAR(stub_handler))

/**
 * LLT_ENTRY_STUB() - define entry stub to funcion @stub_handler
 */
#define LLT_ENTRY_STUB(stub_handler, newfunc)		\
	LLT_ENTRY_STUB_DEFINE(_LLT_ENTRY_STUB_VAR(stub_handler),stub_handler,newfunc)

#define LLT_ENTRY_STUB_LOCAL(stub_handler, newfunc)		\
	static LLT_ENTRY_STUB_DEFINE(_LLT_ENTRY_STUB_VAR(stub_handler),stub_handler,newfunc)

/**
 * LLT_RETURN_STUB() - define return stub to funcion @stub_handler 
 */
#define LLT_RETURN_STUB_WITH_TYPE(stub_handler, rettype, retvalue)		\
	LLT_RETURN_STUB_DEFINE(_LLT_RETURN_STUB_VAR(stub_handler),stub_handler,rettype,retvalue)

#define LLT_RETURN_STUB_WITH_TYPE_LOCAL(stub_handler, rettype, retvalue)		\
	LLT_RETURN_STUB_DEFINE_LOCAL(_LLT_RETURN_STUB_VAR(stub_handler),stub_handler,rettype,retvalue)

#define LLT_RETURN_STUB(stub_handler, retvalue)		\
	LLT_RETURN_STUB_WITH_TYPE(stub_handler, llt_stub_ret_t, retvalue)

#define LLT_RETURN_STUB_LOCAL(stub_handler, retvalue)		\
	LLT_RETURN_STUB_WITH_TYPE_LOCAL(stub_handler, llt_stub_ret_t, retvalue)

/**
 * LLT_REGISTER_ENTRY_STUB() - fail if the entry of @func cannot replace @stub_handler
 * @stub_handler: address expression of old entry
 * @func: address expression of new entry
 */
#define LLT_REGISTER_ENTRY_STUB(stub_handler, func) \
	llt_stub_active_entry(&(_LLT_ENTRY_STUB_VAR(stub_handler)), func)

/**
 * LLT_REGISTER_RETURN_STUB() - fail if the return value of @func cannot replace @value
 * @stub_handler: address expression of entry
 * @value: new return value expression of entry
 */
#define LLT_REGISTER_RETURN_STUB(stub_handler, value) \
	llt_stub_active_return(&(_LLT_RETURN_STUB_VAR(stub_handler)),value)

/**
 * LLT_UNREGISTER_ENTRY_STUB() - restore the old entry of @stub_handler
 * @stub_handler: address expression of old entry
 */
#define LLT_UNREGISTER_ENTRY_STUB(stub_handler) \
	llt_stub_deactive_entry(&(_LLT_ENTRY_STUB_VAR(stub_handler)))

/**
 * LLT_UNREGISTER_ENTRY_STUB() - restore the old return vaule of @stub_handler
 * @stub_handler: address expression of old entry
 */
#define LLT_UNREGISTER_RETURN_STUB(stub_handler) \
	llt_stub_deactive_return(&(_LLT_RETURN_STUB_VAR(stub_handler)))

int llt_stub_clear_all();

/* assert, expect macro define */

/* unit test check flag */
#define _LLT_CK_FAIL_NOP			    (0)
#define _LLT_CK_FAIL_EXIT			(1)

#define _LLT_CK_TYPE_EXPECT			(0)
#define _LLT_CK_TYPE_ASSERT			(1)
#define _LLT_CK_TYPE_TEST			(2)
#define _LLT_CK_TYPE_STUB_ACTIVE		(3)
#define _LLT_CK_TYPE_STUB_DEACTIVE	(4)
#define _LLT_CK_TYPE_STUB_UPDATE		(5)

#define LLT_CK_FLAG(fail_op, type)	((type & 0xFF) | ((fail_op)<<8))
#define LLT_CK_TYPE(flag)  			(flag & 0xFF)
#define LLT_CK_OP(flag)  			(((flag)>>8) & 0xFF)

int _llt_fail_unless (int cond, int flag, const char *file,
			int line, const char *fmt, ...);


char *_llt_mem_to_str(void *d, int size, char *s, int len);

/* Fail the test case unless expr is true */
/* The space before the comma sign before ## is essential to be compatible
   with gcc 2.95.3 and earlier.
*/
#define llt_fail_unless_msg(expr, check_type, format, ...)			\
			_llt_fail_unless(expr, check_type, __FILE__, __LINE__,	\
					format, ## __VA_ARGS__)

#define llt_fail_unless(expr, check_type,...)\
	        _llt_fail_unless(expr, check_type, __FILE__, __LINE__,		\
					"", ## __VA_ARGS__)

#define _llt_check_prefix "Expected: "
#define _llt_check_word_sep " vs "
#define _llt_check_line_sep "\n"
#define _llt_check_block_sep "\n---\n"
/* ==== basic check macro define === */
#define _llt_check_true(ck_type, X, fmt,...) ({ \
		int x = (X); \
		llt_fail_unless_msg(x, ck_type, \
			_llt_check_prefix "'%s == true' : 0x%x " fmt, \
			#X, x, ## __VA_ARGS__); \
	})

#define _llt_check_false(ck_type, X, fmt,...) ({ \
		int x = (X); \
		llt_fail_unless_msg(!x, ck_type, \
			_llt_check_prefix "'%s == false' : 0x%x " fmt, \
			#X, x, ## __VA_ARGS__); \
	})
	
/* Integer comparsion macros with improved output compared to fail_unless(). */
/* O may be any comparion operator. */
#define _llt_check_int(ck_type, X, O, Y, fmt,...) ({ \
		int x = (X); int y = (Y); \
		llt_fail_unless_msg(x O y, ck_type, \
			_llt_check_prefix "'%s "#O" %s' : 0x%x" _llt_check_word_sep "0x%x " fmt, \
			#X, #Y, x, y, ## __VA_ARGS__); \
	})

#define _llt_check_long(ck_type, X, O, Y, fmt,...) ({ \
		long x = (X); long y = (Y); \
		llt_fail_unless_msg(x O y, ck_type, \
			_llt_check_prefix "'%s "#O" %s' : 0x%lx" _llt_check_word_sep "0x%lx " fmt, \
			#X, #Y, x, y, ## __VA_ARGS__); \
	})
	

/* String comparsion macros with improved output compared to fail_unless() */
#define _llt_check_str_eq(ck_type, X, Y, fmt,...) ({ \
		const char* x = (X); const char* y = (Y); \
		int cmpret = (strcmp(x,y) == 0); \
		llt_fail_unless_msg(cmpret, ck_type, \
			_llt_check_prefix "'%s == %s' :" \
			_llt_check_line_sep "(%p)\"%s\"" \
			_llt_check_line_sep "(%p)\"%s\"" \
			_llt_check_line_sep fmt, \
			#X, #Y, x, x, y, y, ## __VA_ARGS__);\
	})

#define _llt_check_str_ne(ck_type, X, Y, fmt,...) ({ \
		const char* x = (X); const char* y = (Y);\
		int cmpret = (strcmp(x,y) != 0); \
		llt_fail_unless_msg(cmpret, ck_type, \
			_llt_check_prefix "'%s != %s' :" \
			_llt_check_line_sep "(%p/%p)\"%s\"" \
			_llt_check_line_sep fmt,\
			#X, #Y, x, y, x, ## __VA_ARGS__);\
	})

/* Memory comparsion macros with improved output compared to fail_unless() */
#define _llt_check_mem_eq(ck_type, X, Y, size, fmt,...) ({ \
		const void* x = (X); const void* y = (Y);int s = (size); \
		char x_str[256]; char y_str[256]; \
		llt_fail_unless_msg(memcmp(x,y,s) == 0, ck_type, \
			_llt_check_prefix "'%s == %s' :" \
			_llt_check_block_sep "(%p)\n%s" \
			_llt_check_block_sep "(%p)\n%s" \
			_llt_check_block_sep fmt, \
			#X, #Y, x, _llt_mem_to_str(x,s,x_str,sizeof(x_str)), \
			y, _llt_mem_to_str(y,s,y_str,sizeof(y_str)), \
			## __VA_ARGS__);\
	})

#define _llt_check_mem_ne(ck_type, X, Y, size, fmt,...) ({ \
		const void* x = (X); const void* y = (Y);int s = (size); \
		char x_str[256]; char y_str[256]; \
		llt_fail_unless_msg(memcmp(x,y,s) != 0, ck_type, \
			_llt_check_prefix "'%s != %s' :" \
			_llt_check_block_sep "(%p/%p)\n%s" \
			_llt_check_block_sep fmt, \
			#X, #Y, x, y, _llt_mem_to_str(x,s,x_str,sizeof(x_str)), \
			## __VA_ARGS__);\
	})


#define LLT_CK_FLAG_ASSERT  LLT_CK_FLAG(_LLT_CK_FAIL_EXIT, _LLT_CK_TYPE_ASSERT)
#define LLT_CK_FLAG_EXPECT  LLT_CK_FLAG(_LLT_CK_FAIL_NOP, _LLT_CK_TYPE_EXPECT)
#define LLT_CK_FLAG_TEST   LLT_CK_FLAG(_LLT_CK_FAIL_NOP, _LLT_CK_TYPE_TEST)

/**
 * TEST_TRUE() - fail and return if @C evaluates to false
 * @C: Boolean expression to evaluate
 */
#define TEST_TRUE(C) \
	_llt_check_true(LLT_CK_FLAG_TEST, (C),"")

#define TEST_TRUE_MSG(C, fmt,...) \
	_llt_check_true(LLT_CK_FLAG_TEST, (C), fmt, ##__VA_ARGS__)

/**
 * TEST_FALSE() - fail and return if @C evaluates to true
 * @C: Boolean expression to evaluate
 */
#define TEST_FALSE(C) \
	_llt_check_false(LLT_CK_FLAG_TEST, (C),"")
#define TEST_FALSE_MSG(C, fmt,...) \
	_llt_check_false(LLT_CK_FLAG_TEST, (C), fmt, ##__VA_ARGS__)

/**
 * TEST_LONG_EQ() - compare two longs, fail and return if @X != @Y
 * @X: Expected value
 * @Y: Actual value
 */
#define TEST_LONG_EQ(X, Y)	\
	_llt_check_long(LLT_CK_FLAG_TEST, X, ==, Y,"")
#define TEST_LONG_EQ_MSG(X, Y, fmt,...) \
	_llt_check_long(LLT_CK_FLAG_TEST, X, ==, Y, fmt, ##__VA_ARGS__)
#define TEST_LONG_NE(X, Y)	\
		_llt_check_long(LLT_CK_FLAG_TEST, X, !=, Y,"")
#define TEST_LONG_NE_MSG(X, Y, fmt,...) \
		_llt_check_long(LLT_CK_FLAG_TEST, X, !=, Y, fmt, ##__VA_ARGS__)
#define TEST_LONG_GE(X, Y)	\
		_llt_check_long(LLT_CK_FLAG_TEST, X, >=, Y,"")
#define TEST_LONG_GE_MSG(X, Y, fmt,...) \
		_llt_check_long(LLT_CK_FLAG_TEST, X, >=, Y, fmt, ##__VA_ARGS__)
#define TEST_ADDR_EQ(X, Y) \
	_llt_check_long(LLT_CK_FLAG_TEST, (u64)(X), ==, (u64)(Y),"")
#define TEST_ADDR_EQ_MSG(X, Y, fmt,...) \
	_llt_check_long(LLT_CK_FLAG_TEST, (u64)(X), ==, (u64)(Y), fmt, ##__VA_ARGS__)
#define TEST_ADDR_NE(X, Y) \
		_llt_check_long(LLT_CK_FLAG_TEST, (u64)(X), !=, (u64)(Y),"")
#define TEST_ADDR_NE_MSG(X, Y, fmt,...) \
		_llt_check_long(LLT_CK_FLAG_TEST, (u64)(X), !=, (u64)(Y), fmt, ##__VA_ARGS__)
#define TEST_ADDR_GE(X, Y) \
		_llt_check_long(LLT_CK_FLAG_TEST, (u64)(X), >=, (u64)(Y),"")
#define TEST_ADDR_GE_MSG(X, Y, fmt,...) \
		_llt_check_long(LLT_CK_FLAG_TEST, (u64)(X), >=, (u64)(Y), fmt, ##__VA_ARGS__)
#define TEST_INT_EQ(X, Y) \
	_llt_check_int(LLT_CK_FLAG_TEST, X, ==, Y,"")
#define TEST_INT_EQ_MSG(X, Y, fmt,...) \
	_llt_check_int(LLT_CK_FLAG_TEST, X, ==, Y, fmt, ##__VA_ARGS__)
#define TEST_INT_GT(X, Y) \
	_llt_check_int(LLT_CK_FLAG_TEST, X, >, Y,"")
#define TEST_INT_GT_MSG(X, Y, fmt,...)	\
	_llt_check_int(LLT_CK_FLAG_TEST, X, >, Y, fmt, ##__VA_ARGS__)
#define TEST_INT_NE(X, Y) \
	_llt_check_int(LLT_CK_FLAG_TEST, X, !=, Y,"")
#define TEST_INT_NE_MSG(X, Y, fmt,...) \
	_llt_check_int(LLT_CK_FLAG_TEST, X, !=, Y, fmt, ##__VA_ARGS__)
#define TEST_INT_GE(X, Y) \
		_llt_check_int(LLT_CK_FLAG_TEST, X, >=, Y,"")
#define TEST_INT_GE_MSG(X, Y, fmt,...) \
		_llt_check_int(LLT_CK_FLAG_TEST, X, >=, Y, fmt, ##__VA_ARGS__)


/**
 * ASSERT_TRUE() - fail and return if @C evaluates to false
 * @C: Boolean expression to evaluate
 */
#define ASSERT_TRUE(C) \
	_llt_check_true(LLT_CK_FLAG_ASSERT, (C),"")

#define ASSERT_TRUE_MSG(C, fmt,...) \
	_llt_check_true(LLT_CK_FLAG_ASSERT, (C), fmt, ##__VA_ARGS__)


/**
 * ASSERT_FALSE() - fail and return if @C evaluates to true
 * @C: Boolean expression to evaluate
 */
#define ASSERT_FALSE(C) \
	_llt_check_false(LLT_CK_FLAG_ASSERT, (C),"")
#define ASSERT_FALSE_MSG(C, fmt,...) \
	_llt_check_false(LLT_CK_FLAG_ASSERT, (C), fmt, ##__VA_ARGS__)

/**
 * ASSERT_LONG_EQ() - compare two longs, fail and return if @X != @Y
 * @X: Expected value
 * @Y: Actual value
 */
#define ASSERT_LONG_EQ(X, Y)	\
	_llt_check_long(LLT_CK_FLAG_ASSERT, X, ==, Y,"")
#define ASSERT_LONG_EQ_MSG(X, Y, fmt,...) \
	_llt_check_long(LLT_CK_FLAG_ASSERT, X, ==, Y, fmt, ##__VA_ARGS__)
#define ASSERT_LONG_NE(X, Y)	\
		_llt_check_long(LLT_CK_FLAG_ASSERT, X, !=, Y,"")
#define ASSERT_LONG_NE_MSG(X, Y, fmt,...) \
		_llt_check_long(LLT_CK_FLAG_ASSERT, X, !=, Y, fmt, ##__VA_ARGS__)
#define ASSERT_LONG_GE(X, Y)	\
		_llt_check_long(LLT_CK_FLAG_ASSERT, X, >=, Y,"")
#define ASSERT_LONG_GE_MSG(X, Y, fmt,...) \
		_llt_check_long(LLT_CK_FLAG_ASSERT, X, >=, Y, fmt, ##__VA_ARGS__)
#define ASSERT_ADDR_EQ(X, Y) \
	_llt_check_long(LLT_CK_FLAG_ASSERT, (u64)(X), ==, (u64)(Y),"")
#define ASSERT_ADDR_EQ_MSG(X, Y, fmt,...) \
	_llt_check_long(LLT_CK_FLAG_ASSERT, (u64)(X), ==, (u64)(Y), fmt, ##__VA_ARGS__)
#define ASSERT_ADDR_NE(X, Y) \
		_llt_check_long(LLT_CK_FLAG_ASSERT, (u64)(X), !=, (u64)(Y),"")
#define ASSERT_ADDR_NE_MSG(X, Y, fmt,...) \
		_llt_check_long(LLT_CK_FLAG_ASSERT, (u64)(X), !=, (u64)(Y), fmt, ##__VA_ARGS__)
#define ASSERT_ADDR_GE(X, Y) \
		_llt_check_long(LLT_CK_FLAG_ASSERT, (u64)(X), >=, (u64)(Y),"")
#define ASSERT_ADDR_GE_MSG(X, Y, fmt,...) \
		_llt_check_long(LLT_CK_FLAG_ASSERT, (u64)(X), >=, (u64)(Y), fmt, ##__VA_ARGS__)
#define ASSERT_INT_EQ(X, Y) \
	_llt_check_int(LLT_CK_FLAG_ASSERT, X, ==, Y,"")
#define ASSERT_INT_EQ_MSG(X, Y, fmt,...) \
	_llt_check_int(LLT_CK_FLAG_ASSERT, X, ==, Y, fmt, ##__VA_ARGS__)
#define ASSERT_INT_GT(X, Y) \
	_llt_check_int(LLT_CK_FLAG_ASSERT, X, >, Y,"")
#define ASSERT_INT_GT_MSG(X, Y, fmt,...)	\
	_llt_check_int(LLT_CK_FLAG_ASSERT, X, >, Y, fmt, ##__VA_ARGS__)
#define ASSERT_INT_NE(X, Y) \
	_llt_check_int(LLT_CK_FLAG_ASSERT, X, !=, Y,"")
#define ASSERT_INT_NE_MSG(X, Y, fmt,...) \
	_llt_check_int(LLT_CK_FLAG_ASSERT, X, !=, Y, fmt, ##__VA_ARGS__)
#define ASSERT_INT_GE(X, Y) \
		_llt_check_int(LLT_CK_FLAG_ASSERT, X, >=, Y,"")
#define ASSERT_INT_GE_MSG(X, Y, fmt,...) \
		_llt_check_int(LLT_CK_FLAG_ASSERT, X, >=, Y, fmt, ##__VA_ARGS__)

#define ASSERT_INT_LT(X, Y) \
		_llt_check_int(LLT_CK_FLAG_ASSERT, X, <, Y,"")
#define ASSERT_INT_LT_MSG(X, Y, fmt,...) \
		_llt_check_int(LLT_CK_FLAG_ASSERT, X, <, Y, fmt, ##__VA_ARGS__)

#define ASSERT_INT_LE(X, Y) \
			_llt_check_int(LLT_CK_FLAG_ASSERT, X, <=, Y,"")
#define ASSERT_INT_LE_MSG(X,Y, fmt,...) \
			_llt_check_int(LLT_CK_FLAG_ASSERT, X, <=, Y, fmt, ##__VA_ARGS__)

#define ASSERT_STR_EQ(X, Y) \
	_llt_check_str_eq(LLT_CK_FLAG_ASSERT, X, Y,"")
#define ASSERT_STREQ_MSG(X, Y, fmt,...) \
	_llt_check_str_eq(LLT_CK_FLAG_ASSERT, X, Y, fmt, ##__VA_ARGS__)
#define ASSERT_STR_NE(X, Y) \
	_llt_check_str_ne(LLT_CK_FLAG_ASSERT, X, Y,"")
#define ASSERT_STRNE_MSG(X, Y, fmt,...) \
	_llt_check_str_ne(LLT_CK_FLAG_ASSERT, X, Y, fmt, ##__VA_ARGS__)


#define ASSERT_MEM_EQ(X, Y, S) \
	_llt_check_mem_eq(LLT_CK_FLAG_ASSERT, X, Y, S,"")
#define ASSERT_MEMEQ_MSG(X, Y, S, fmt,...) \
	_llt_check_mem_eq(LLT_CK_FLAG_ASSERT, X, Y, S, fmt, ##__VA_ARGS__)
#define ASSERT_MEM_NE(X, Y, S) \
	_llt_check_mem_ne(LLT_CK_FLAG_ASSERT, X, Y, S,"")
#define ASSERT_MEMNE_MSG(X, Y, S, fmt,...) \
	_llt_check_mem_ne(LLT_CK_FLAG_ASSERT, X, Y, S, fmt, ##__VA_ARGS__)

/**
 * EXPECT_TRUE() - fail and return if @C evaluates to false
 * @C: Boolean expression to evaluate
 *
 */
#define EXPECT_TRUE(C) \
	_llt_check_true(LLT_CK_FLAG_EXPECT, (C),"")
#define EXPECT_TRUE_MSG(C, fmt,...) \
	_llt_check_true(LLT_CK_FLAG_EXPECT, (C), fmt, ##__VA_ARGS__)

/**
 * EXPECT_FALSE() - fail and return if @C evaluates to true
 * @C: Boolean expression to evaluate
 */
#define EXPECT_FALSE(C) \
	_llt_check_false(LLT_CK_FLAG_EXPECT, (C),"")
#define EXPECT_FALSE_MSG(C, fmt,...) \
	_llt_check_false(LLT_CK_FLAG_EXPECT, (C), fmt, ##__VA_ARGS__)

/**
 * EXPECT_LONG_EQ() - compare two longs, fail and return if @X != @Y
 * @X: Expected value
 * @Y: Actual value
 */
#define EXPECT_LONG_EQ(X, Y) \
	_llt_check_long(LLT_CK_FLAG_EXPECT, X, ==, Y,"")
#define EXPECT_LONG_EQ_MSG(X, Y, fmt,...) \
	_llt_check_long(LLT_CK_FLAG_EXPECT, X, ==, Y, fmt, ##__VA_ARGS__)
#define EXPECT_LONG_NE(X, Y)	\
		_llt_check_long(LLT_CK_FLAG_EXPECT, X, !=, Y,"")
#define EXPECT_LONG_NE_MSG(X, Y, fmt,...) \
		_llt_check_long(LLT_CK_FLAG_EXPECT, X, !=, Y, fmt, ##__VA_ARGS__)
#define EXPECT_LONG_GE(X, Y)	\
		_llt_check_long(LLT_CK_FLAG_EXPECT, X, >=, Y,"")
#define EXPECT_LONG_GE_MSG(X, Y, fmt,...) \
		_llt_check_long(LLT_CK_FLAG_EXPECT, X, >=, Y, fmt, ##__VA_ARGS__)
#define EXPECT_ADDR_EQ(X, Y)	\
	_llt_check_long(LLT_CK_FLAG_EXPECT, (u64)(X), ==, (u64)(Y),"")
#define EXPECT_ADDR_EQ_MSG(X, Y, fmt,...) \
	_llt_check_long(LLT_CK_FLAG_EXPECT, (u64)(X), ==, (u64)(Y), fmt, ##__VA_ARGS__)
#define EXPECT_ADDR_NE(X, Y)	\
		_llt_check_long(LLT_CK_FLAG_EXPECT, (u64)(X), !=, (u64)(Y),"")
#define EXPECT_ADDR_NE_MSG(X, Y, fmt,...) \
		_llt_check_long(LLT_CK_FLAG_EXPECT, (u64)(X), !=, (u64)(Y), fmt, ##__VA_ARGS__)
#define EXPECT_ADDR_GE(X, Y)	\
		_llt_check_long(LLT_CK_FLAG_EXPECT, (u64)(X), >=, (u64)(Y),"")
#define EXPECT_ADDR_GE_MSG(X, Y, fmt,...) \
		_llt_check_long(LLT_CK_FLAG_EXPECT, (u64)(X), >=, (u64)(Y), fmt, ##__VA_ARGS__)
#define EXPECT_INT_EQ(X, Y) \
	_llt_check_int(LLT_CK_FLAG_EXPECT, X, ==, Y,"")
#define EXPECT_INT_EQ_MSG(X, Y, fmt,...) \
	_llt_check_int(LLT_CK_FLAG_EXPECT, X, ==, Y, fmt, ##__VA_ARGS__)
#define EXPECT_INT_GT(X, Y) \
		_llt_check_int(LLT_CK_FLAG_EXPECT, X, >, Y,"")
#define EXPECT_INT_GT_MSG(X, Y, fmt,...) \
		_llt_check_int(LLT_CK_FLAG_EXPECT, X, >, Y, fmt, ##__VA_ARGS__)
#define EXPECT_INT_LT(X, Y) \
		_llt_check_int(LLT_CK_FLAG_EXPECT, X, <, Y,"")
#define EXPECT_INT_LT_MSG(X, Y, fmt,...) \
		_llt_check_int(LLT_CK_FLAG_EXPECT, X, <, Y, fmt, ##__VA_ARGS__)
#define EXPECT_INT_NE(X, Y) \
		_llt_check_int(LLT_CK_FLAG_EXPECT, X, !=, Y,"")
#define EXPECT_INT_NE_MSG(X,Y, fmt,...) \
		_llt_check_int(LLT_CK_FLAG_EXPECT, X, !=, Y, fmt, ##__VA_ARGS__)
#define EXPECT_INT_GE(X, Y) \
		_llt_check_int(LLT_CK_FLAG_EXPECT, X, >=, Y,"")
#define EXPECT_INT_GE_MSG(X,Y, fmt,...) \
		_llt_check_int(LLT_CK_FLAG_EXPECT, X, >=, Y, fmt, ##__VA_ARGS__)
#define EXPECT_INT_LE(X, Y) \
			_llt_check_int(LLT_CK_FLAG_EXPECT, X, <=, Y,"")
#define EXPECT_INT_LE_MSG(X,Y, fmt,...) \
			_llt_check_int(LLT_CK_FLAG_EXPECT, X, <=, Y, fmt, ##__VA_ARGS__)

#define EXPECT_STR_EQ(X, Y) \
	_llt_check_str_eq(LLT_CK_FLAG_EXPECT, X, Y,"")
#define EXPECT_STR_EQ_MSG(X, Y, fmt,...) \
	_llt_check_str_eq(LLT_CK_FLAG_EXPECT, X, Y, fmt, ##__VA_ARGS__)
#define EXPECT_STR_NE(X, Y) \
	_llt_check_str_ne(LLT_CK_FLAG_EXPECT, X, Y,"")
#define EXPECT_STR_NE_MSG(X, Y, fmt,...) \
	_llt_check_str_ne(LLT_CK_FLAG_EXPECT, X, Y, fmt, ##__VA_ARGS__)

#define EXPECT_MEM_EQ(X, Y, S) \
	_llt_check_mem_eq(LLT_CK_FLAG_EXPECT, X, Y, S,"")
#define EXPECT_MEM_EQ_MSG(X, Y, S, fmt,...) \
	_llt_check_mem_eq(LLT_CK_FLAG_EXPECT, X, Y, S, fmt, ##__VA_ARGS__)
#define EXPECT_MEM_NE(X, Y, S) \
	_llt_check_mem_ne(LLT_CK_FLAG_EXPECT, X, Y, S,"")
#define EXPECT_MEM_NE_MSG(X, Y, S, fmt,...) \
	_llt_check_mem_ne(LLT_CK_FLAG_EXPECT, X, Y, S, fmt, ##__VA_ARGS__)

/* param list interface */
#define LLT_ARGS_LAST_ONE(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, ...) a11
// Note: With GCC, the preprocessor eliminates the comma in
//   , ## __VA_ARGS__
// IF __VA_ARGS__ is empty. (https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html)
// This is necessary to correctly count the arguments
//
#define LLT_ARGS_COUNT(...) LLT_ARGS_LAST_ONE(dummy, ## __VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define LLT_ARGS_PRINT(...) llt_printf("%-40s has %d arguments\n", #__VA_ARGS__, LLT_ARGS_COUNT(__VA_ARGS__))

/* mocker default interface */
#define mocker(fn, most_cnt, ret_val)	 mocker_p4(#fn, fn, most_cnt, ret_val)
#define mocker_ret(fn, ret0, ret1, ret2) mocker_ret_p4(#fn, fn, ret0, ret1, ret2)
#define mocker_invoke(fn, stub, most)	 mocker_invoke_p4(#fn, #stub, fn, stub, most)
void mocker_clean();

/* mocker stub interface */
#define MOCKER_APIs_DECLARE(n) \
void mocker_p##n(char *nameOfCaller, void *addrOfCaller, int most, int ret);\
void mocker_ret_p##n(char *nameOfCaller, void *addrOfCaller, int ret0, int ret1, int ret2);\
void mocker_invoke_p##n(char *nameOfCaller, char *nameOfstub, void *addrOfCaller, void *addrOfStub, int most)

MOCKER_APIs_DECLARE(0);
MOCKER_APIs_DECLARE(1);
MOCKER_APIs_DECLARE(2);
MOCKER_APIs_DECLARE(3);
MOCKER_APIs_DECLARE(4);
MOCKER_APIs_DECLARE(5);
MOCKER_APIs_DECLARE(6);
MOCKER_APIs_DECLARE(7);
MOCKER_APIs_DECLARE(8);
MOCKER_APIs_DECLARE(9);
MOCKER_APIs_DECLARE(10);
MOCKER_APIs_DECLARE(11);
MOCKER_APIs_DECLARE(12);
#endif
