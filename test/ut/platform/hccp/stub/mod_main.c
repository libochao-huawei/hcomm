/*
 * test case  post compile 
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>

#include "ut_lib.h"

static void *ut_malloc_trace_common(struct ut_list_head *mem_list, int size, int oob_size, int flags, 
					const char *file, int line, const char *fmt, va_list ap);
static int ut_mem_check_and_free(struct ut_list_head *list);

int ut_main(int argc, char **argv);
int ut_testcase_log(const char *szFormat, ...);
#define TC_FAIL 0
#define TC_PASS 1
void ut_testcase_expect_result(int result);
void ut_testcase_assert_result(int result);

#ifdef CONFIG_UT_MODULE
#include <linux/module.h>
struct module __this_module = {
	.name = KBUILD_MODNAME,
};
#endif

#define LLT_STRUCTOR_MAX  256
struct llt_module_ctor {
	int (*fn)();
	char tags[128];
};
static struct llt_module_ctor lltModCtor[LLT_STRUCTOR_MAX];
static int lltModCtorCnt = 0;
void llt_register_ctor(int (*fn)(), char *tags)
{
	int i = lltModCtorCnt;
	if (i < LLT_STRUCTOR_MAX) {
		struct llt_module_ctor *ctor = &(lltModCtor[i]);
		lltModCtorCnt++;
		ctor->fn = fn;
		strncpy(ctor->tags, tags, sizeof(ctor->tags));
	} else {
		printf("ERROR:Cannot reg ctor %d for %s\n", lltModCtorCnt, tags);
	}
}

int llt_ctor_exec(int argc, char **argv)
{
	int i = 0;
	int ret = 0;
	int failcount = 0;

	for (i = 0; i < lltModCtorCnt; i++) {
		struct llt_module_ctor *ctor = &(lltModCtor[i]);
		if (!ctor->fn)
			continue;
		printf("<INIT> %s\n", ctor->tags);
		ret = ctor->fn();
		if (ret) {
			failcount++;
			printf("<WARN-%d:%d> %s ret=%d.\n", failcount, i, ctor->tags, ret);
		}
	}
	printf("<INIT> total %d:%d.\n", lltModCtorCnt, failcount);

	return 0;
}

struct llt_module_dtor {
	void (*fn)();
	char tags[128];
};
static struct llt_module_dtor lltModDtor[LLT_STRUCTOR_MAX];
static int lltModDtorCnt = 0;

void llt_register_dtor(void (*fn)(), char *tags)
{
	int i = lltModDtorCnt;
	if (i < LLT_STRUCTOR_MAX) {
		struct llt_module_dtor *dtor = &(lltModDtor[i]);
		lltModDtorCnt++;
		dtor->fn = fn;
		strncpy(dtor->tags, tags, sizeof(dtor->tags));
	} else {
		printf("ERROR:Cannot reg dtor %d for %s\n", lltModDtorCnt, tags);
	}
}

void llt_dtor_exec(int argc, char **argv)
{
	int i = 0;
	int ret = 0;

	for (i = 0; i < lltModDtorCnt; i++) {
		struct llt_module_dtor *dtor = &(lltModDtor[i]);
		if (!dtor->fn)
			continue;
		printf("<EXIT:%d> %s\n", i, dtor->tags);
		dtor->fn();
	}
}

#define LLT_CLOCK_MAX  4

struct llt_module_clock {
	void (*fn)(void*);
	char tags[128];
	void *data;
	int id;
	int interval;
	int count;
	int state;
	pthread_mutex_t lock;
};
static struct llt_module_clock lltModClock[LLT_CLOCK_MAX];
static int lltModClockCnt = 0;

int llt_register_clock(void (*fn)(void *), int nsec, void *data, char *name)
{
	int i = lltModClockCnt;
	if (i < LLT_CLOCK_MAX) {
		struct llt_module_clock *clk = &(lltModClock[i]);
		clk->state = 0;
		clk->fn = fn;
		clk->interval = nsec;
		clk->data = data;
		clk->id = lltModClockCnt;
		pthread_mutex_init(&clk->lock, NULL);
		strncpy(clk->tags, name, sizeof(clk->tags));
		lltModClockCnt++;
		return clk->id;
	} else {
		printf("ERROR:Cannot reg clock %d for %s\n", lltModClockCnt, name);
	}

	return 0;
}

static llt_exec_clock(struct llt_module_clock *clk, int state)
{
	if (!clk->fn)
		return;
	pthread_mutex_lock(&clk->lock);
	clk->count++;
	if (clk->count == clk->interval)
		clk->count = 0;
	clk->state = state;
	if (state && (clk->count == 0))
		clk->fn(clk->data);
	pthread_mutex_unlock(&clk->lock);
}

void llt_set_clockstate(int id, int state)
{
	int start = 0;
	int end = lltModClockCnt;
	int ret = 0;

	if (id < 0 || id >= end ) {
		start = 0;
	} else {
		start = id;
		end = id;
	}
	for (; start < end; start++) {
		struct llt_module_clock *clk = &(lltModClock[start]);
		if (start == clk->id)  {
			printf("<TIMER-%d:%d> %s state => %d\n", start, clk->interval, clk->tags, state);
			llt_exec_clock(clk, state);
		}
	}
}
#define LLT_TEST_TYPE_NAME "llt unit test"


static struct ut_list_head ut_mem_alloc_global_whitelist = UT_LIST_HEAD_INIT(ut_mem_alloc_global_whitelist);
static struct ut_list_head ut_mem_alloc_list = UT_LIST_HEAD_INIT(ut_mem_alloc_list);

/* llt module init/exit */
/* 101 : llt test init/exit */
/* 102~108   : linux-param */
/* 109~116   : module-param */
/* 117(0~17) : kernel init/exit */
static void __attribute((constructor(101))) llt_main_init(void)
{
	lltModCtorCnt = 0;
	lltModDtorCnt = 0;
	lltModClockCnt = 0;
	printf(_CONSOLE_DISP_BLUE "start kernel module %s : %s." _CONSOLE_DISP_NONE "\n",
		LLT_TEST_TYPE_NAME, KBUILD_MODNAME);
}

static void __attribute((destructor(101))) llt_main_deinit(void)
{
	ut_mem_check_and_free(&ut_mem_alloc_global_whitelist);
	ut_mem_check_and_free(&ut_mem_alloc_list);

	printf(_CONSOLE_DISP_BLUE "finished kernel module %s : %s." _CONSOLE_DISP_NONE "\n",
		LLT_TEST_TYPE_NAME, KBUILD_MODNAME);
}

static void llt_setup_signal_handler(int argc, char *argv[]);

int main(int argc, char **argv)
{
	int ret = 0;

	llt_setup_signal_handler(argc, argv);
	ret = llt_ctor_exec(argc, argv);
	if (ret)
		return ret;
	ret = ut_main(argc, argv);
	llt_dtor_exec(argc, argv);
	return ret;
}


static void llt_signalHandler(int signo)
{
	int i = 0;
	switch (signo) {
	case SIGALRM :
		for (i = 0; i < lltModClockCnt; i++) {
			struct llt_module_clock *clk = &(lltModClock[i]);
			if (i == clk->id)
				llt_exec_clock(clk, clk->state);
		}
	break;
	}
}
 
static void llt_setup_signal_handler(int argc, char *argv[])
{
    signal(SIGALRM, llt_signalHandler);
    struct itimerval new_value, old_value;
    new_value.it_value.tv_sec = 0;
    new_value.it_value.tv_usec = 10000;//10ms
    new_value.it_interval.tv_sec = 1; //1s
    new_value.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &new_value, &old_value);
}

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#ifndef container_of
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) * __mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)); })
#endif


#define UT_DEBUG
/* support alloc zero size memory */
//#define UT_SUPPORT_ALLOC_ZERO_SIZE
//#define UT_TRACE_STUB
//#define UT_DEBUG_STUB
//#define UT_TRACE_CNT_RANGE
//#define UT_TRACE_KEYVAL_MAP
#define UT_LOG_BIT(x) (1 << (x))
//#define UT_CNT_RANGE_LOG_FLAGS  UT_LOG_BIT(_UT_CNT_RANGE_FLAG_READ)
//#define UT_KEYVAL_MAP_LOG_FLAGS UT_LOG_BIT(_UT_KEYVAL_MAP_FLAG_READ)

#define UT_DUMPSTACK
#define UT_DUMP_DEEP 100

int ut_mem_error()
{
	return ut_mem_check_and_free(&ut_mem_alloc_list);
}

void *ut_malloc_trace(int size, int oob_size, int flags, 
						const char *file, int line, const char *fmt, ...)
{
	void * ptr;
	va_list ap;

	va_start(ap, fmt);
	ptr = ut_malloc_trace_common(&ut_mem_alloc_list, size, oob_size, flags, 
		file, line, fmt, ap);
	va_end(ap);

	return ptr;
}
/* bugfixes : NCS core lib exists memory leak error */
void *NCSInnerMalloc(int size)
{
	return ut_malloc_trace_common(&ut_mem_alloc_global_whitelist, size, 0, UT_MEM_FLAG_NOCHECK,
		__FILE__, __LINE__, "NCSCORE-MALLOC", 0);
}
void NCSInnerFree(void *ptr)
{
	ut_free_trace(ptr, 0, __FILE__, __LINE__, "NCSCORE-FREE");
}

static int ut_vprintf(const char *fmt, char *last, va_list args)
{
	int len;
#define MAX_PRINTF_CHARS 4096
	static char buf[MAX_PRINTF_CHARS];
	
	len = 0;
	len = vsnprintf(buf, MAX_PRINTF_CHARS-1,fmt,args);
	buf[len] = 0;
#undef MAX_PRINTF_CHARS
	if (len > 0) {
		len = ut_testcase_log("%s", buf);
		if (last)
			*last = buf[len - 1];
	}

	return len;
}

static inline char *_ut_get_filename(char *filePath)
{
	int len;
	int pos;
	char c;

	if (!filePath)
		return "??";

	len = strlen(filePath);
	if (len > 0)
		pos = len - 1;
	else
		pos = 0;
	while(pos > 0) {
		c = filePath[pos];
		if (('/' == c) || ('\\' == c))
			return filePath + pos + 1;
		pos--;
	}
	return filePath + pos;
}

/* unit test malloc */
struct ut_mem_node {
	struct ut_list_head list;
	int size;
	int flags;
	char trace_info[512];
	void *oob_data;
};

#define ut_mem_ptr_to_node(ptr) ((struct ut_mem_node *)(ptr) - 1)
#define ut_mem_note_to_ptr(node) ((struct ut_mem_node *)(node) + 1)
static int ut_mem_check_and_free(struct ut_list_head *list)
{
	struct ut_list_head *node, *next;
	int err = 0;

	ut_list_for_each_safe(node, next, list) {
		struct ut_mem_node *mem_node = container_of(node, struct ut_mem_node, list);
		void *p = mem_node + 1;
		
		if (mem_node->oob_data)
			free(mem_node->oob_data);

		if (!(UT_MEM_FLAG_NOCHECK & mem_node->flags)) {
			err++;
			/* resource leak */
			ut_printf("RES-LEAK[%d] %p~%p %5x %s\n", 
				err, p, p + mem_node->size, mem_node->size,
					mem_node->trace_info);
		}

		ut_list_del(&(mem_node->list));
		free(mem_node);
	}
	
	ut_init_list_head(list);
	
	return err;
}

static void *ut_malloc_trace_common(struct ut_list_head *mem_list, int size, int oob_size, int flags, 
					const char *file, int line, const char *fmt, va_list ap)
{
	struct ut_mem_node *node;
	void *p = NULL;

#ifndef UT_SUPPORT_ALLOC_ZERO_SIZE
	if (!size)
		return NULL;
#endif

	node = malloc(sizeof(struct ut_mem_node) + size);
	if (node) {
		int ret = 0;
		int free_len = 0;
		char *buf;
		
		memset(node, 0, sizeof(*node));
		ut_list_add(&(node->list), mem_list);

		free_len = sizeof(node->trace_info);
		buf = node->trace_info;
		ret = snprintf(buf, free_len, "%s:%i ", _ut_get_filename(file), line);
		if (ret && fmt && ap) {
			free_len -= ret;
			buf += ret;
			if (free_len > 0) {
				ret = vsnprintf(buf, free_len, fmt, ap);
			}
		}
		node->trace_info[sizeof(node->trace_info) - 1] = 0;

		if (oob_size > 0)
			node->oob_data = malloc(oob_size);
		p = ut_mem_note_to_ptr(node);;
		if (UT_MEM_FLAG_ZERO & flags) {
			if (size > 0)
				memset(p, 0, size);
			if (node->oob_data)
				memset(node->oob_data, 0, oob_size);
		}
		node->flags = flags;
		node->size = size;
	} else {
		ut_printf("[MEM-ALLOC-FAIL] size=%d+%d %s:%i\n", 
			size, oob_size, file?file:"??", line);
	}
	
	return p;
}

void *ut_mem_oob_data(void *p)
{
	struct ut_mem_node *node = ut_mem_ptr_to_node(p);

	return node->oob_data;
}

void ut_free_trace(void *p, int flags, const char *file, int line, const char *fmt, ...)
{
	struct ut_mem_node *mem_node;

	if (!p)
		return;

	/* if nocheck flags is true, this means the buffer be managed by auto free */
	if(UT_MEM_FLAG_NOCHECK & flags) {
		return;
	}

	mem_node = (struct ut_mem_node *)p - 1;
	if (mem_node->oob_data)
		free(mem_node->oob_data);
	
	ut_list_del(&(mem_node->list));
	free(mem_node);
}


/* unit test map */
struct ut_map_node {
	struct ut_list_head list;
	struct ut_entry e;
};

static inline void _ut_map_log(struct ut_map_node *e_node, int id, char *name, int flags)
{
#ifdef UT_KEYVAL_MAP_LOG_FLAGS
	struct ut_mem_node *mem_node = ut_mem_ptr_to_node(e_node);
	struct ut_entry *e = &(e_node->e);
	int op = flags & 0xf;
	char *tips;
	char idRefBuf[16];
	char idCurBuf[16];
	char *all_titles[_UT_KEYVAL_MAP_FLAG_SIZE] = {
		"TEST  ",
		"ACTIVE",
		"CLEAR ",
	};

	if (!(UT_KEYVAL_MAP_LOG_FLAGS & UT_LOG_BIT(op))) {
		return;
	}

	if (op < _UT_KEYVAL_MAP_FLAG_SIZE)
		tips = all_titles[op];
	else
		tips = "UNKNOW";

	if (id < 0) {
		sprintf(idCurBuf, "%s", "*");
	} else {
		sprintf(idCurBuf, "%d", id);
	}

	if (e->id < 0) {
		sprintf(idRefBuf, "%s", "*");
	} else {
		sprintf(idRefBuf, "%d", e->id);
	}

	ut_printf("MAP(%s:%s) "_CONSOLE_DISP_CYAN "%-s" _CONSOLE_DISP_NONE " %s => %s\n",
			idCurBuf, idRefBuf, tips, name, mem_node->trace_info);
#endif
}

static inline void _ut_map_trace(struct ut_map *m, char *file, int line, const char *fmt, ...)
{
#ifdef UT_TRACE_KEYVAL_MAP
	int len;
	va_list ap;
#define MAX_MAP_TRACE_CHARS 128
	char buf[MAX_MAP_TRACE_CHARS];

	va_start(ap, fmt);

	len = 0;
	len = vsnprintf(buf, sizeof(buf) - 1,fmt,ap);
	va_end(ap);
	buf[len] = 0;

	ut_printf("MAP " _CONSOLE_DISP_BLUE "%-s" _CONSOLE_DISP_NONE " %s => %s:%d\n",
				buf, m->name, _ut_get_filename(file), line);
#endif
}


int ut_map_clear(struct ut_map *m, char *file, int line)
{
	struct ut_list_head *node, *next;
	int cnt = 0;
	
	ut_list_for_each_safe(node, next, &(m->entrys)) {
		struct ut_map_node *e_node = container_of(node, struct ut_map_node, list);

		cnt++;
		_ut_map_log(e_node, cnt, m->name, _UT_KEYVAL_MAP_FLAG_DEL);

		ut_list_del(&(e_node->list));
		ut_free_trace(e_node, 0, file, line, NULL);
	}
	
	ut_init_list_head(&(m->entrys));
	_ut_map_trace(m, file, line, "RESET");
	return cnt;
}

struct ut_entry *ut_map_find(struct ut_map *m, struct ut_entry *item, int flags, char *file, int line)
{
	struct ut_list_head *node, *next;

	ut_list_for_each_safe(node, next, &(m->entrys)) {
		struct ut_map_node * e_node = container_of(node, struct ut_map_node, list);
		struct ut_entry *e = NULL;
		if (item->key == e_node->e.key) {
			if (item->id == -1)
				e = &(e_node->e);
			else if (item->id == e_node->e.id)
				e = &(e_node->e);
			if (e) {
				_ut_map_log(e_node, item->id, m->name, flags);
				return e;
			}
		}
	}

	if (_UT_KEYVAL_MAP_FLAG_READ & flags)
		_ut_map_trace(m, file, line, "CHECK(%d)[%x,%x]", item->id, item->key);

	return NULL;
}

struct ut_entry *ut_map_insert(struct ut_map *m, const struct ut_entry *item, char *file, int line)
{
	struct ut_map_node *e_node = NULL;

	e_node = ut_malloc_trace(sizeof(struct ut_map_node), 0, 0, 
		file, line, "[%x,%x]", item->key, item->data);
	
	if (e_node) {
		e_node->e.id = item->id;
		e_node->e.key = item->key;
		e_node->e.data = item->data;
		ut_list_add(&(e_node->list), &(m->entrys));
		
		_ut_map_trace(m, file, line, "ADD[%x,%x]", item->key, item->data);
		return &(e_node->e);
	}

	return NULL;
}

struct ut_entry *ut_map_erase(struct ut_map *m, struct ut_entry *item, char *file, int line)
{
	struct ut_entry *find = ut_map_find(m, item, _UT_KEYVAL_MAP_FLAG_DEL, file, line);
	
	if (find) {
		struct ut_map_node *e_node = container_of(find, struct ut_map_node, e);

		item->data = find->data;
		
		_ut_map_trace(m, file, line, "DEL[%x,%x]", item->key, item->data);
		ut_list_del(&(e_node->list));
		ut_free_trace(e_node, 0, file, line, NULL);

		return item;
	}
	
	return NULL;
}

/* unit test counter range */
struct ut_range_node {
	struct ut_list_head list;
	int base;
	int size;
};


static inline void _ut_cnt_range_log(struct ut_range_node *range, int val, char *name, int flags)
{
#ifdef UT_CNT_RANGE_LOG_FLAGS
	struct ut_mem_node *mem_node = ut_mem_ptr_to_node(range);
	int op = flags & 0xf;
	char *tips;
	char *all_titles[_UT_CNT_RANGE_FLAG_SIZE] = {
		"TEST  ",
		"ACTIVE",
		"SET   ",
		"CLEAR ",
	};

	if (!(UT_CNT_RANGE_LOG_FLAGS & UT_LOG_BIT(op))) {
		return;
	}

	if (op < _UT_CNT_RANGE_FLAG_SIZE)
		tips = all_titles[op];
	else
		tips = "UNKNOW";

	ut_printf("COUNTER(%d) " _CONSOLE_DISP_GREEN "%-s" _CONSOLE_DISP_NONE " %s => %s\n",
				val, tips, name, mem_node->trace_info);
#endif
}

static inline void _ut_cnt_range_trace(struct ut_cnt_range *counter_range, char *file, int line, const char *fmt, ...)
{
#ifdef UT_TRACE_CNT_RANGE
	int len;
	va_list ap;
#define MAX_RANGE_TRACE_CHARS 128
	char buf[MAX_RANGE_TRACE_CHARS];

	va_start(ap, fmt);

	len = 0;
	len = vsnprintf(buf, sizeof(buf) - 1,fmt,ap);
	va_end(ap);
	buf[len] = 0;

	ut_printf("COUNTER(%d) " _CONSOLE_DISP_BLUE "%-s" _CONSOLE_DISP_NONE " %s => %s:%d\n",
				counter_range->counter, buf, counter_range->name, _ut_get_filename(file), line);
#endif
}


void _ut_cnt_range_append(struct ut_cnt_range *counter_range, int start, int end, char *file, int line)
{
	struct ut_range_node *range = ut_malloc_trace(sizeof(struct ut_range_node), 0, 0,
				file, line, "[%d:%d]", start, end);

	if (range) {
		int min = (start < end) ? start : end;
		int max = (start > end) ? start : end;
		range->base = min;
		range->size = (max - min) + 1;
		ut_list_add(&(range->list), &(counter_range->ranges));
		_ut_cnt_range_trace(counter_range, file, line, "ADD[%d:%d]", start, end);
	}
}

int _ut_cnt_range_check(struct ut_cnt_range *counter_range, int step, int *ptr_count, char *file, int line)
{
	struct ut_list_head *node, *next;
	int val;
	int once_flag = 1;
	
	ut_list_for_each_safe(node, next, &(counter_range->ranges)) {
		struct ut_range_node *range = container_of(node, struct ut_range_node, list);
		if (once_flag) {
			counter_range->counter += step;
			val = counter_range->counter;
			if (ptr_count) {
				*ptr_count = val;
			}
			once_flag = 0;
		}

		if (range->size > 0)
			if (((range->base + range->size) > val) && (val >= range->base)) {
				_ut_cnt_range_log(range, val, counter_range->name, _UT_CNT_RANGE_FLAG_READ);
				return 1;
			}
	}
	_ut_cnt_range_trace(counter_range, file, line, "CHECK");

	return 0;
}

void _ut_cnt_range_clear(struct ut_cnt_range *counter_range, char *file, int line)
{
	struct ut_list_head *node, *next;
	int cnt = 0;
	
	ut_list_for_each_safe(node, next, &(counter_range->ranges)) {
		struct ut_range_node *range = container_of(node, struct ut_range_node, list);

		_ut_cnt_range_log(range, cnt++, counter_range->name, _UT_CNT_RANGE_FLAG_DEL);

		ut_list_del(&(range->list));
		ut_free_trace(range, 0, file, line, NULL);
	}
	counter_range->counter = 0;
	ut_init_list_head(&(counter_range->ranges));
	_ut_cnt_range_trace(counter_range, file, line, "RESET");
}

/* unit test common */
int ut_printf(const char *fmt, ...)
{
	int ret = 0;
	va_list ap;
	va_start(ap, fmt);
	ret = ut_vprintf(fmt, NULL, ap);
	va_end(ap);

	return ret;
}

char *_ut_mem_to_str(void *d, int size, char *s, int len)
{
	char *buf = s;
	unsigned char *data = d;
	int ret;
	int total_len = 0;
	int count = 0;
	int newline = 0;

	while ((total_len < len) && (size > 0)) {
		ret = snprintf(buf + total_len, len, "%02x%s", *data, newline ? "\n" : " ");
		if (ret < 0)
			break;
		total_len += ret;
		data++;
		size--;
		
		if (count < 16) {
			count++;
			newline = 0;
		} else {
			newline = 1;
			count = 0;
		}
	}

	return s;
}

void _ut_dumpstack(void)
{
	int j, nptrs;
	void *buffer[UT_DUMP_DEEP];
	char **strings;

	nptrs = backtrace(buffer, UT_DUMP_DEEP);
	strings = backtrace_symbols(buffer, nptrs);
	printf("backtrace:\n");
	if (strings != NULL) {
		for (j = 0; j < nptrs; j++)
			printf("%s\n", strings[j]);
		
		free(strings);
	}
}

int _ut_fail_unless (int cond, int flag, const char *file,
			int line, const char *fmt, ...)
{
	va_list ap;
	char c;
	int check_type = UT_CK_TYPE(flag);
	int check_op = UT_CK_OP(flag);
	char *name;
	int ret = 0;

	/* ok */
	if (cond) {
		if (_UT_CK_TYPE_EXPECT == check_type)
			ut_testcase_expect_result(TC_PASS);

		return cond;
	}

	/* fail */
	switch (check_type) {
	case _UT_CK_TYPE_TEST:
		name = "TEST";
		break;
		
	case _UT_CK_TYPE_EXPECT:
		name = "EXPECT";
		ut_testcase_expect_result(TC_FAIL);
		break;
	case _UT_CK_TYPE_ASSERT:
		name = "ASSERT";
		break;
	case _UT_CK_TYPE_STUB_ACTIVE:
		name = "STUB ACTIVE";
		ut_testcase_assert_result(TC_FAIL);
		break;
	case _UT_CK_TYPE_STUB_DEACTIVE:
		name = "STUB DEACTIVE";
		break;
	case _UT_CK_TYPE_STUB_UPDATE:
		name = "STUB UPDATE";
		break;
	default:
		name = "";
		break;
	}
	
#ifdef UT_DEBUG
	ut_printf("[%s " _CONSOLE_DISP_RED "FAIL" _CONSOLE_DISP_NONE "] %s:%i.\n", name, _ut_get_filename(file), line);
#endif
	va_start(ap, fmt);
	ut_vprintf(fmt, &c, ap);
	va_end(ap);
	if (c != '\n')
		ut_printf("\n");

	switch (check_op) {
	case _UT_CK_FAIL_EXIT:
#ifdef UT_DUMPSTACK
		_ut_dumpstack();
#endif
		//exit(-1);
		break;
	default:
		break;
	}
	
	return cond;
}

#ifdef UT_TRACE_STUB
#define _ut_stub_register_info(tag, act, handlename, currentname, newstub_name) do { \
		ut_printf("[" _CONSOLE_DISP_CYAN "%s" _CONSOLE_DISP_NONE " %s] %s : %s%s%s\n", tag, act, handlename, currentname, \
			newstub_name ? " => " : "", newstub_name ? newstub_name : "");\
	} while(0)
#define _ut_stub_unregister_info(tag, act, handlname, currentname, newstub_name) do { \
		ut_printf("[" _CONSOLE_DISP_CYAN "%s" _CONSOLE_DISP_NONE " %s] %s : %s%s%s\n", tag, act, handlname, currentname, \
			newstub_name ? " <= " : "", newstub_name ? newstub_name : "");\
	} while(0)
#else
#define _ut_stub_register_info(tag, act, handlename, currentname, newstub_name) do{}while(0)
#define _ut_stub_unregister_info(tag, act, handlename, currentname, newstub_name) do{}while(0)
#endif

int KKSetStubULData(ut_stub_fn_t pFuncAddr, ut_stub_ret_t ulData);
int KKInstallStub(ut_stub_fn_t pOldFuncAddr, void *pUserAPIAddr, int iStubType);
int KKInstallULDataStub(ut_stub_fn_t pFuncAddr, ut_stub_ret_t ulData);
int KKUninstallStub(ut_stub_fn_t pOldFuncAddr);

#ifdef UT_DEBUG_STUB
#define _ut_stub_init_debug(title, origineAddr, handlename, newValue)	do { \
			ut_printf("[" _CONSOLE_DISP_YELLOW "%s" _CONSOLE_DISP_NONE "] %p : %x, %s.\n", \
				title, origineAddr, newValue, handlename);\
		} while(0)
#else
#define _ut_stub_init_debug(title, origineAddr, handlename, newValue) do{}while(0)
#endif


struct stub_fn_node {
	struct ut_list_head list;
	void * addr;
	enum {
		STUB_STATUS_USED = 0x2018,
	} state;
};

static struct stub_fn_node ut_stub_fn_cache[256] = { 0 };
static inline struct stub_fn_node *stub_fn_note_alloc(void *addr)
{
	int i = 0;
	struct stub_fn_node *fn_node = NULL;

	for (i = 0; i < sizeof(ut_stub_fn_cache) / sizeof(ut_stub_fn_cache[0]); i++) {
		fn_node = &(ut_stub_fn_cache[i]);
		if (fn_node->state == 0) {
			fn_node->state = STUB_STATUS_USED;
			fn_node->addr = addr;
			return fn_node;
		}
	}

	return NULL;
}

static inline void stub_fn_note_free(struct stub_fn_node *fn_node)
{
	if (!fn_node)
		return;

	memset(fn_node, 0, sizeof(*fn_node));
}

struct stub_fn_node *stub_fn_node_search(struct ut_list_head *list, void *addr)
{
	struct ut_list_head *node, *next;

	ut_list_for_each_safe(node, next, list) {
		struct stub_fn_node *fn_node = container_of(node, struct stub_fn_node, list);
		if ((fn_node->state == STUB_STATUS_USED) && (fn_node->addr == addr))
			return fn_node;
	}
	return NULL;
}

static struct ut_list_head ut_stub_fn_list = UT_LIST_HEAD_INIT(ut_stub_fn_list);
static inline int stub_fn_list_test_and_add(ut_stub_fn_t fn)
{
	struct stub_fn_node *fn_node = stub_fn_node_search(&ut_stub_fn_list, fn);

	if (fn_node) {
		return 1;
	}

	fn_node = stub_fn_note_alloc(fn);
	ut_list_add(&(fn_node->list), &ut_stub_fn_list);

	return 0;
}

static inline int stub_fn_list_test_and_del(ut_stub_fn_t fn)
{
	struct stub_fn_node *fn_node = stub_fn_node_search(&ut_stub_fn_list, fn);

	if (fn_node) {
		ut_list_del(&(fn_node->list));
		stub_fn_note_free(fn_node);
		return 1;
	}

	return 0;
}

int ut_stub_entry_install(ut_stub_fn_t originFunc, ut_stub_fn_t newFunc, char *name)
{
	if (NULL == originFunc)
		return 0;

	/*if (stub_fn_list_test_and_add(originFunc)) {
		_ut_stub_init_debug("entry***", originFunc, name, newFunc);
		(void)KKUninstallStub(originFunc);
	} else {
		_ut_stub_init_debug("entry+++", originFunc, name, newFunc);
	}

	if (KKInstallStub(originFunc, newFunc, 1))
		return 0;

	return -1;*/
	mocker_u64_invoke(originFunc, newFunc, 512);
	return 0;
}

int ut_stub_entry_uninstall(ut_stub_fn_t originFunc, ut_stub_fn_t curFunc, char *name)
{
	/*if (stub_fn_list_test_and_del(originFunc)) {
		_ut_stub_init_debug("entry---", originFunc, name, curFunc);
		(void)KKUninstallStub(originFunc);
	}*/
	mocker_clean();
	return 0;
}

int ut_stub_return_install(ut_stub_fn_t func, ut_stub_ret_t newRet, char *name)
{
	if (NULL == func)
		return 0;
/*
	if (stub_fn_list_test_and_add(func)) {
		_ut_stub_init_debug("return***", func, name, newRet);
		if (KKSetStubULData(func, newRet))
			return 0;
			
	} else {
		_ut_stub_init_debug("return+++", func, name, newRet);
		if (KKInstallULDataStub(func, newRet))
			return 0;
	}

	return -2;*/
	mocker_u64(func, 512, newRet);
	return 0;
}

int ut_stub_return_uninstall(ut_stub_fn_t func, ut_stub_ret_t curRet, char *name)
{
	if (NULL == func)
		return 0;

	/*if (stub_fn_list_test_and_del(func)) {
		_ut_stub_init_debug("return---", func, name, curRet);
		(void)KKUninstallStub(func);
	}*/
	
	mocker_clean();

	return 0;
}

int ut_stub_clear_all()
{
	mocker_clean();
	return 0;
}

int _ut_stub_register(struct ut_stub_handle *stub_handle, struct ut_stub_data *newstub, char * f, int line)
{
	int ret = 0;
	struct ut_stub_data *body = &(stub_handle->body);
	if (stub_handle->func && body->val.func \
		&& newstub) {
		struct ut_stub_data *stub = &stub_handle->stub;
		int useAgent = (stub_handle->func != body->val.func);
		char *title = "STUB NA";

		if (stub->type != newstub->type) {
			ut_printf("Stub error: %s:%i %s(%d) %s(%d) from %s:%i\n", __FUNCTION__, __LINE__,
				stub->name, stub->type, newstub->name, stub->type, f, line);
			exit(0);
		}
		
		if (UT_STUB_TYPE_ENTRY == stub->type) {
			title = "STUB ENT";
			ret = ut_stub_entry_install(stub_handle->func,
						newstub->val.func,
						stub_handle->init.name);
		} else if (UT_STUB_TYPE_RETURN == stub->type) {
			if (useAgent) {
				ut_stub_ret_t *retAddr = (ut_stub_ret_t *)(stub_handle->init.val.ret);
				title = "STUB AGT";

				*retAddr = newstub->val.ret;
				ret = ut_stub_entry_install(stub_handle->func,
							body->val.func,
							stub_handle->init.name);
			} else {
				title = "STUB RET";
				ret = ut_stub_return_install(stub_handle->func,
						newstub->val.ret,
						stub_handle->init.name);
			}
		} else
			ret = -100;

		if (0 == ret) {
			_ut_stub_register_info(title, ">>>", stub_handle->init.name, newstub->name, NULL);
			if (stub != newstub)
				*stub = *newstub;
		} else {
			/* error */
			(void)_ut_fail_unless(0, UT_CK_FLAG(_UT_CK_FAIL_NOP, _UT_CK_TYPE_STUB_ACTIVE), f, line,
				"%s %s => %s\n", title, stub_handle->init.name, newstub->name);
		}
	}

	return ret;
}

void _ut_stub_unregister(struct ut_stub_handle *stub_handle, char * f, int line)
{
	struct ut_stub_data *body = &(stub_handle->body);

	if (stub_handle->func && (body->val.func)) {
		struct ut_stub_data *stub = &stub_handle->stub;
		int useAgent = (stub_handle->func != body->val.func);
		int ret;
		char *title = "STUB NA";

		if (UT_STUB_TYPE_ENTRY == stub->type) {
			title = "STUB ENT";
			ret = ut_stub_entry_uninstall(stub_handle->func,
				stub_handle->init.val.func,
				stub_handle->init.name);
		} else if (UT_STUB_TYPE_RETURN == stub->type) {
			if (useAgent) {
				title = "STUB AGT";
				ret = ut_stub_entry_uninstall(stub_handle->func,
					body->val.func,
					stub_handle->init.name);
			} else {
				title = "STUB RET";
				ret = ut_stub_return_uninstall(stub_handle->func,
							stub_handle->init.val.ret, 
							stub_handle->init.name);
			}
		} else {
			ret = -101;
		}
		
		if (0 == ret)
			_ut_stub_unregister_info(title, "<<<<", stub_handle->init.name, stub->name, NULL);
		else
			(void)_ut_fail_unless(0, UT_CK_FLAG(_UT_CK_FAIL_NOP, _UT_CK_TYPE_STUB_DEACTIVE), f, line,
				"%s %s <= %s\n", title, stub_handle->init.name, stub->name);
	}
}

