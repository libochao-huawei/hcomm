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

//#include "KKLib.h"
#include "ut_lib.h"
#include "llt_common.h"

#ifdef CONFIG_UT_MODULE
#include <linux/module.h>
struct module __this_module = {
	.name = KBUILD_MODNAME,
};
#endif
typedef unsigned int t_mem_size;
typedef unsigned long long t_mem_ptr;

static void *llt_malloc_trace_common(struct llt_list_head *mem_list, t_mem_size size,
				     t_mem_size oob_size, t_mem_size aligned_shift, int flags,
				     const char *file, int line, const char *fmt, va_list ap);
static int llt_mem_check_and_free(struct llt_list_head *list);

int llt_testcase_log(const char *szFormat, ...);
#define TC_FAIL 0
#define TC_PASS 1
void llt_testcase_expect_result(int result);
void llt_testcase_assert_result(int result);
int llt_testcase_main(int argc, char **argv);

static struct llt_list_head llt_mem_alloc_global_whitelist = ST_LIST_HEAD_INIT(llt_mem_alloc_global_whitelist);
static struct llt_list_head llt_mem_alloc_list = ST_LIST_HEAD_INIT(llt_mem_alloc_list);

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
		int nr = i + 1;
		if (!ctor->fn)
			continue;
		printf("<INIT-%02d> %s\n", nr, ctor->tags);
		ret = ctor->fn();
		if (ret) {
			failcount++;
			printf("<INIT-%02d:WARN-%d> %s ret=%d.\n", nr,
				failcount,ctor->tags, ret);
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

	if (lltModDtorCnt < 1)
		return;
	for (i = 0; i < lltModDtorCnt; i++) {
		int nr = lltModDtorCnt - 1 - i;
		struct llt_module_dtor *dtor = &(lltModDtor[nr]);
		if (!dtor->fn)
			continue;
		printf("<EXIT:%02d> %s\n", nr, dtor->tags);
		dtor->fn();
		printf("<DONE:%02d> %s\n", nr, dtor->tags);
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
#ifdef CONFIG_LLT_ST
		clk->state = 1;
#else
		clk->state = 0;
#endif
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

#ifdef CONFIG_LLT_ST
#define LLT_TEST_TYPE_NAME "llt system test"
#else
#define LLT_TEST_TYPE_NAME "llt unit test"
#endif
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
	llt_mem_check_and_free(&llt_mem_alloc_global_whitelist);
	llt_mem_check_and_free(&llt_mem_alloc_list);

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
	ret = llt_testcase_main(argc, argv);
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

/*** START DEBUG CFG ****/
#define LLT_DEBUG
/* support alloc zero size memory */
#define LLT_SUPPORT_ALLOC_ZERO_SIZE
#define LLT_TRACE_STUB
//#define LLT_TRACE_MEM
#define LLT_DEBUG_STUB

#define LLT_DUMPSTACK
#define LLT_DUMP_DEEP 100

#define LLT_LOG_BIT(x) (1 << (x))

/* unit test cnt range operation flags */
#define _LLT_CNT_RANGE_FLAG_TEST 0
#define _LLT_CNT_RANGE_FLAG_READ 1
#define _LLT_CNT_RANGE_FLAG_ADD  2
#define _LLT_CNT_RANGE_FLAG_DEL  3
#define _LLT_CNT_RANGE_FLAG_SIZE 4

#define LLT_TRACE_CNT_RANGE
#define LLT_CNT_RANGE_LOG_FLAGS  (LLT_LOG_BIT(_LLT_CNT_RANGE_FLAG_ADD) |\
				  LLT_LOG_BIT(_LLT_CNT_RANGE_FLAG_DEL))
//#define LLT_CNT_RANGE_LOG_FLAGS  LLT_LOG_BIT(_LLT_CNT_RANGE_FLAG_READ)

/* map operation flags */
#define _LLT_KEYVAL_MAP_FLAG_TEST 0
#define _LLT_KEYVAL_MAP_FLAG_READ 1
#define _LLT_KEYVAL_MAP_FLAG_ADD  2
#define _LLT_KEYVAL_MAP_FLAG_DEL  3
#define _LLT_KEYVAL_MAP_FLAG_RST  4
#define _LLT_KEYVAL_MAP_FLAG_SIZE 5

#define LLT_TRACE_KEYVAL_MAP
#define LLT_KEYVAL_MAP_LOG_FLAGS (LLT_LOG_BIT(_LLT_KEYVAL_MAP_FLAG_RST) | \
				  LLT_LOG_BIT(_LLT_KEYVAL_MAP_FLAG_ADD) | \
				  LLT_LOG_BIT(_LLT_KEYVAL_MAP_FLAG_DEL))
//#define LLT_KEYVAL_MAP_LOG_FLAGS LLT_LOG_BIT(_LLT_KEYVAL_MAP_FLAG_READ)

/*** END DEBUG CFG ****/

int llt_mem_error()
{
	return llt_mem_check_and_free(&llt_mem_alloc_list);
}

void *llt_malloc_align_trace(int size, int oob_size, int align_shift, int flags,
			     const char *file, int line, const char *fmt, ...)
{
	void * ptr;
	va_list ap;

	va_start(ap, fmt);
	ptr = llt_malloc_trace_common(&llt_mem_alloc_list, size, oob_size,
				      align_shift, flags, file, line, fmt, ap);
	va_end(ap);

	return ptr;
}
/* bugfixes : NCS core lib exists memory leak error */
void *NCSInnerMalloc(int size)
{
	return llt_malloc_trace_common(&llt_mem_alloc_global_whitelist, size, 0,
				       0, LLT_MEM_FLAG_NOCHECK, __FILE__,
				       __LINE__, "NCSCORE-MALLOC", 0);
}
void NCSInnerFree(void *ptr)
{
	llt_free_trace(ptr, 0, __FILE__, __LINE__, "NCSCORE-FREE");
}

static int llt_vprintf(const char *fmt, char *last, va_list args)
{
	int len;
#define MAX_PRINTF_CHARS 4096
	static char buf[MAX_PRINTF_CHARS];
	
	len = 0;
	len = vsnprintf(buf, MAX_PRINTF_CHARS-1,fmt,args);
	buf[len] = 0;
#undef MAX_PRINTF_CHARS
	if (len > 0) {
		len = llt_testcase_log("%s", buf);
		if (last)
			*last = buf[len - 1];
	}

	return len;
}


#define _llt_get_filename(filePath) llt_get_filename(filePath, 0)


/* unit test malloc */
struct llt_mem_node {
	struct llt_list_head list;
	t_mem_size size;
	t_mem_size align_shift;
	t_mem_size oob_size;
	int flags;
	char trace_info[512];
	void *oob_data;
	t_mem_ptr align_ptr;
};

#define llt_mem_ptr_to_node(ptr) ((struct llt_mem_node *)(*((t_mem_ptr *)((t_mem_ptr)ptr - sizeof(t_mem_ptr)))))
static int llt_mem_check_and_free(struct llt_list_head *list)
{
	struct llt_list_head *node, *next;
	int err = 0;

	llt_list_for_each_safe(node, next, list) {
		struct llt_mem_node *mem_node = container_of(node, struct llt_mem_node, list);
		
		if (mem_node->oob_data)
			free(mem_node->oob_data);

		if (!(LLT_MEM_FLAG_NOCHECK & mem_node->flags)) {
			err++;
			#ifdef LLT_TRACE_MEM
			/* resource leak */
			llt_printf("MEM[%d] %llx~%llx %5x %s\n",
				err, mem_node->align_ptr,
				mem_node->align_ptr + mem_node->size,
				mem_node->size, mem_node->trace_info);
			#endif
		}

		llt_list_del(&(mem_node->list));
		free(mem_node);
	}
	
	llt_init_list_head(list);
	
	return err;
}

/* LLT ALLOC BLOCK LAYOUT
 --------------------> head
 NODE [sizeof(struct llt_mem_node)]
 + oob_ptr
 + size
 + align_shift
  --------------------
 PADDING  [aligned_size or 0]
 --------------------
 head_ptr [sizeof(void *)]
 --------------------> user_ptr
 DATA [size]
 --------------------
 */
static void *llt_malloc_trace_common(struct llt_list_head *mem_list, t_mem_size size, 
				     t_mem_size oob_size, t_mem_size aligned_shift, int flags,
				     const char *file, int line, const char *fmt, va_list ap)
{
	void *p = NULL;
	t_mem_size offset;
	t_mem_size aligned_byte;
	unsigned long long addr;
	struct llt_mem_node *node;

#ifndef LLT_SUPPORT_ALLOC_ZERO_SIZE
	if (!size)
		return NULL;
#endif

	if (aligned_shift > 0)
		aligned_byte = (1 << aligned_shift);
	else
		aligned_byte = 1;

	offset = sizeof(t_mem_ptr) + (aligned_byte - 1);
	node = malloc(sizeof(struct llt_mem_node) + size + offset);
	if (node) {
		int ret = 0;
		int free_len = 0;
		char *buf;

		memset(node, 0, sizeof(*node));
		llt_list_add(&(node->list), mem_list);

		free_len = sizeof(node->trace_info);
		buf = node->trace_info;
		ret = snprintf(buf, free_len, "%s:%i ", _llt_get_filename(file), line);
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
		p = (void *)(node + 1);
		addr = (t_mem_ptr)p;
		addr = addr + offset;
		/* align the address */
		if (aligned_shift > 0) {
			addr >>= aligned_shift;
			addr <<= aligned_shift;
		}
		p = (void *)addr;
		*((t_mem_ptr *)(addr - sizeof(t_mem_ptr))) = (t_mem_ptr)node;
		if (LLT_MEM_FLAG_ZERO & flags) {
			if (size > 0)
				memset(p, 0, size);
			if (node->oob_data)
				memset(node->oob_data, 0, oob_size);
		}
		node->flags = flags;
		node->size = size;
		node->align_shift = aligned_shift;
		node->oob_size = oob_size;
		node->align_ptr = addr;
	} else {
		llt_printf("[MEM-ALLOC-FAIL] size=%d+%d %s:%i\n", 
			size, oob_size, file?file:"??", line);
	}
	
	return p;
}

void *llt_mem_oob_data(void *p)
{
	struct llt_mem_node *node = llt_mem_ptr_to_node(p);

	return node->oob_data;
}

int llt_mem_size(void *p)
{
	struct llt_mem_node *node = llt_mem_ptr_to_node(p);

	return node->size;
}

void llt_free_trace(void *p, int flags, const char *file, int line, const char *fmt, ...)
{
	struct llt_mem_node *mem_node;

	if (!p)
		return;

	/* if nocheck flags is true, this means the buffer be managed by auto free */
	if(LLT_MEM_FLAG_NOCHECK & flags) {
		return;
	}

	mem_node = llt_mem_ptr_to_node(p);
	if (mem_node->oob_data)
		free(mem_node->oob_data);
	
	llt_list_del(&(mem_node->list));
	free(mem_node);
}

/* unit test map */
struct llt_map_node {
	struct llt_list_head list;
	struct llt_map_entry e;
};

static inline void _llt_map_log(struct llt_map_node *e_node, int id, char *name, int flags)
{
#ifdef LLT_KEYVAL_MAP_LOG_FLAGS
	struct llt_mem_node *mem_node = llt_mem_ptr_to_node(e_node);
	struct llt_map_entry *e = &(e_node->e);
	int op = flags & 0xf;
	char *tips;
	char idRefBuf[16];
	char idCurBuf[16];
	char *all_titles[_LLT_KEYVAL_MAP_FLAG_SIZE] = {
		"TEST  ",
		"ACTIVE",
		"ADD   ",
		"DEL   ",
		"CLEAR ",
	};

	if (!(LLT_KEYVAL_MAP_LOG_FLAGS & LLT_LOG_BIT(op))) {
		return;
	}

	if (op < _LLT_KEYVAL_MAP_FLAG_SIZE)
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

	llt_printf("MAP(%s:%s) "_CONSOLE_DISP_CYAN "%-s" _CONSOLE_DISP_NONE " %s => %s\n",
			idCurBuf, idRefBuf, tips, name, mem_node->trace_info);
#endif
}

static void _llt_map_trace(struct llt_map *m, int flags, char *file, int line, const char *fmt, ...)
{
#ifdef LLT_TRACE_KEYVAL_MAP
	int len;
	va_list ap;
#define MAX_MAP_TRACE_CHARS 128
	char buf[MAX_MAP_TRACE_CHARS];
	int op = flags & 0xf;

	if (!(LLT_KEYVAL_MAP_LOG_FLAGS & LLT_LOG_BIT(op))) {
		return;
	}

	va_start(ap, fmt);

	len = 0;
	len = vsnprintf(buf, sizeof(buf) - 1,fmt,ap);
	va_end(ap);
	buf[len] = 0;

	llt_printf("MAP " _CONSOLE_DISP_BLUE "%-s" _CONSOLE_DISP_NONE " %s => %s:%d\n",
				buf, m->name, _llt_get_filename(file), line);
#endif
}


int llt_map_clear(struct llt_map *m, char *file, int line)
{
	struct llt_list_head *node, *next;
	int cnt = 0;
	
	llt_list_for_each_safe(node, next, &(m->entrys)) {
		struct llt_map_node *e_node = container_of(node, struct llt_map_node, list);

		cnt++;
		_llt_map_log(e_node, cnt, m->name, _LLT_KEYVAL_MAP_FLAG_DEL);

		llt_list_del(&(e_node->list));
		llt_free_trace(e_node, 0, file, line, NULL);
	}
	
	llt_init_list_head(&(m->entrys));
	_llt_map_trace(m, _LLT_KEYVAL_MAP_FLAG_RST, file, line, "RESET");
	return cnt;
}

static int llt_map_key_default_match(const void *p, const void *q)
{
	return p == q;
}

static struct llt_map_entry * llt_map_entry_search(struct llt_map *m, struct llt_map_entry *item)
{
	struct llt_list_head *node, *next;
	int (*match_fn)(const void *, const void *);

	if (m->match_func)
		match_fn = m->match_func;
	else
		match_fn = llt_map_key_default_match;

	llt_list_for_each_safe(node, next, &(m->entrys)) {
		struct llt_map_node * e_node = container_of(node, struct llt_map_node, list);
		struct llt_map_entry *e = NULL;
		if (match_fn(item->key, e_node->e.key)) {
			if ((item->id == -1) || (item->id == e_node->e.id))
				return &(e_node->e);
		}
	}

	return NULL;
}

struct llt_map_entry *llt_map_entry_find(struct llt_map *m, struct llt_map_entry *item, void **v, char *file, int line)
{
	struct llt_list_head *node, *next;
	int flags = v ? _LLT_KEYVAL_MAP_FLAG_READ : _LLT_KEYVAL_MAP_FLAG_TEST;

	struct llt_map_entry * e = llt_map_entry_search(m, item);

	if (e) {
		if (v) *v = e->data;
		_llt_map_log(e, item->id, m->name, flags);
		return e;
	}
	_llt_map_trace(m, flags, file, line, "CHECK(%d)[%x,%x]", item->id, item->key);

	return NULL;
}

struct llt_map_entry *llt_map_entry_insert(struct llt_map *m, const struct llt_map_entry *item, char *file, int line)
{
	struct llt_map_node *e_node = NULL;

	e_node = llt_malloc_trace(sizeof(struct llt_map_node), 0, 0, 
		file, line, "[%x,%x]", item->key, item->data);
	
	if (e_node) {
		e_node->e.id = item->id;
		e_node->e.key = item->key;
		e_node->e.data = item->data;
		llt_list_add(&(e_node->list), &(m->entrys));
		
		_llt_map_trace(m, _LLT_KEYVAL_MAP_FLAG_ADD, file, line, "ADD[%x,%x]", item->key, item->data);
		return &(e_node->e);
	}

	return NULL;
}

struct llt_map_entry *llt_map_entry_erase(struct llt_map *m, struct llt_map_entry *item, char *file, int line)
{
	struct llt_map_entry *find = llt_map_entry_search(m, item);
	
	if (find) {
		struct llt_map_node *e_node = container_of(find, struct llt_map_node, e);

		item->data = find->data;
		
		_llt_map_trace(m, _LLT_KEYVAL_MAP_FLAG_DEL, file, line, "DEL[%x,%x]", item->key, item->data);
		llt_list_del(&(e_node->list));
		llt_free_trace(e_node, 0, file, line, NULL);

		return item;
	}
	
	return NULL;
}

/* test counter range */
struct llt_range_node {
	struct llt_list_head list;
	int base;
	int size;
};

static void _llt_cnt_range_log(struct llt_range_node *range, int val, char *name, int flags)
{
#ifdef LLT_CNT_RANGE_LOG_FLAGS
	struct llt_mem_node *mem_node = llt_mem_ptr_to_node(range);
	int op = flags & 0xf;
	char *tips;
	char *all_titles[_LLT_CNT_RANGE_FLAG_SIZE] = {
		"TEST  ",
		"ACTIVE",
		"SET   ",
		"CLEAR ",
	};

	if (!(LLT_CNT_RANGE_LOG_FLAGS & LLT_LOG_BIT(op))) {
		return;
	}

	if (op < _LLT_CNT_RANGE_FLAG_SIZE)
		tips = all_titles[op];
	else
		tips = "UNKNOW";

	llt_printf("COUNTER(%d) " _CONSOLE_DISP_GREEN "%-s" _CONSOLE_DISP_NONE " %s => %s\n",
				val, tips, name, mem_node->trace_info);
#endif
}

static void _llt_cnt_range_trace(struct llt_cnt_range *counter_range, int flags, char *file, int line, const char *fmt, ...)
{
#ifdef LLT_TRACE_CNT_RANGE
	int len;
	va_list ap;
#define MAX_RANGE_TRACE_CHARS 128
	char buf[MAX_RANGE_TRACE_CHARS];
	int op = flags & 0xf;

	if (!(LLT_CNT_RANGE_LOG_FLAGS & LLT_LOG_BIT(op))) {
		return;
	}

	va_start(ap, fmt);

	len = 0;
	len = vsnprintf(buf, sizeof(buf) - 1,fmt,ap);
	va_end(ap);
	buf[len] = 0;

	llt_printf("COUNTER(%d) " _CONSOLE_DISP_BLUE "%-s" _CONSOLE_DISP_NONE " %s => %s:%d\n",
				counter_range->counter, buf, counter_range->name, _llt_get_filename(file), line);
#endif
}

void _llt_cnt_range_append(struct llt_cnt_range *counter_range, int start, int end, char *file, int line)
{
	struct llt_range_node *range = llt_malloc_trace(sizeof(struct llt_range_node), 0, 0,
				file, line, "[%d:%d]", start, end);

	if (range) {
		int min = (start < end) ? start : end;
		int max = (start > end) ? start : end;
		range->base = min;
		range->size = (max - min) + 1;
		llt_list_add(&(range->list), &(counter_range->ranges));
		_llt_cnt_range_trace(counter_range, _LLT_CNT_RANGE_FLAG_ADD, file, line, "ADD[%d:%d]", start, end);
	}
}

int _llt_cnt_range_check(struct llt_cnt_range *counter_range, int step, int *ptr_count, char *file, int line)
{
	struct llt_list_head *node, *next;
	int val;
	int once_flag = 1;
	
	llt_list_for_each_safe(node, next, &(counter_range->ranges)) {
		struct llt_range_node *range = container_of(node, struct llt_range_node, list);
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
				_llt_cnt_range_log(range, val, counter_range->name, _LLT_CNT_RANGE_FLAG_READ);
				return 1;
			}
	}
	_llt_cnt_range_trace(counter_range, _LLT_CNT_RANGE_FLAG_READ, file, line, "CHECK");

	return 0;
}

void _llt_cnt_range_clear(struct llt_cnt_range *counter_range, char *file, int line)
{
	struct llt_list_head *node, *next;
	int cnt = 0;
	
	llt_list_for_each_safe(node, next, &(counter_range->ranges)) {
		struct llt_range_node *range = container_of(node, struct llt_range_node, list);

		_llt_cnt_range_log(range, cnt++, counter_range->name, _LLT_CNT_RANGE_FLAG_DEL);

		llt_list_del(&(range->list));
		llt_free_trace(range, 0, file, line, NULL);
	}
	counter_range->counter = 0;
	llt_init_list_head(&(counter_range->ranges));
	_llt_cnt_range_trace(counter_range, _LLT_CNT_RANGE_FLAG_DEL, file, line, "RESET");
}

/* unit test common */
int llt_printf(const char *fmt, ...)
{
	int ret = 0;
	va_list ap;
	va_start(ap, fmt);
	ret = llt_vprintf(fmt, NULL, ap);
	va_end(ap);

	return ret;
}

char *_llt_mem_to_str(void *d, int size, char *s, int len)
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

void _llt_dumpstack(void)
{
	int j, nptrs;
	void *buffer[LLT_DUMP_DEEP];
	char **strings;

	nptrs = backtrace(buffer, LLT_DUMP_DEEP);
	strings = backtrace_symbols(buffer, nptrs);
	printf("backtrace:\n");
	if (strings != NULL) {
		for (j = 0; j < nptrs; j++)
			printf("%s\n", strings[j]);
		
		free(strings);
	}
}


void dump_stack()
{
	_llt_dumpstack();
}

int _llt_fail_unless (int cond, int flag, const char *file,
			int line, const char *fmt, ...)
{
	va_list ap;
	char c;
	int check_type = LLT_CK_TYPE(flag);
	int check_op = LLT_CK_OP(flag);
	char *name;
	char *fail_tips = _CONSOLE_DISP_RED "FAIL" _CONSOLE_DISP_NONE;
	int ret = 0;

	/* ok */
	if (cond) {
		if (_LLT_CK_TYPE_EXPECT == check_type)
			llt_testcase_expect_result(TC_PASS);

		return cond;
	}

	/* fail */
	switch (check_type) {
	case _LLT_CK_TYPE_TEST:
		name = "TEST";
		fail_tips = "FALSE";
		break;
		
	case _LLT_CK_TYPE_EXPECT:
		name = "EXPECT";
		llt_testcase_expect_result(TC_FAIL);
		break;
	case _LLT_CK_TYPE_ASSERT:
		name = "ASSERT";
		break;
	case _LLT_CK_TYPE_STUB_ACTIVE:
		name = "STUB ACTIVE";
		llt_testcase_assert_result(TC_FAIL);
		break;
	case _LLT_CK_TYPE_STUB_DEACTIVE:
		name = "STUB DEACTIVE";
		break;
	case _LLT_CK_TYPE_STUB_UPDATE:
		name = "STUB UPDATE";
		break;
	default:
		name = "";
		break;
	}
	
#ifdef LLT_DEBUG
	llt_printf("[%s %s] %s:%i.\n", name, fail_tips, _llt_get_filename(file), line);
#endif
	va_start(ap, fmt);
	//llt_vprintf(fmt, &c, ap);
	va_end(ap);
	if (c != '\n')
		llt_printf("\n");

	switch (check_op) {
	case _LLT_CK_FAIL_EXIT:
#ifdef LLT_DUMPSTACK
		_llt_dumpstack();
#endif
		//exit(-1);
		break;
	default:
		break;
	}
	
	return cond;
}

#define UT_DEBUG
/* support alloc zero size memory */
//#define UT_SUPPORT_ALLOC_ZERO_SIZE
//#define UT_TRACE_STUB

#define UT_DUMPSTACK
#define UT_DUMP_DEEP 100

/*void HLLT_RunAllTC();

int main(int argc,char* argv[])
{
	int en_run_all = 1;
	int en_run_single = 0;

	if (argc > 1)
		en_run_all = atoi(argv[1]);

	if (argc > 2)
		en_run_single = atoi(argv[2]);

	if (en_run_all)
		HLLT_RunAllTC();
	
	while (en_run_single)
	{
		char sz[32] = {0};
		unsigned char *cmd = (unsigned char *)sz;
		scanf("%s", cmd);
		if (0 == strcmp((char *)cmd,"exit"))
		{
			break;
		}
	}
	return  0;
}*/


int ut_vprintf(const char *fmt, va_list args)
{
	int len;
#define MAX_PRINTF_CHARS 4096
	static char buf[MAX_PRINTF_CHARS];
	
	len = 0;
	len = vsnprintf(buf, MAX_PRINTF_CHARS-1,fmt,args);
	buf[len] = 0;
#undef MAX_PRINTF_CHARS
	//if (len > 0)
		//len = KKLog("%s", buf);
	
	return len;
}


#define _ut_print_va_list(fmt) ({\
	int ret = 0;\
	if (fmt) {	\
		va_list ap; \
		va_start(ap, fmt); \
		ret = ut_vprintf(fmt, ap); \
		va_end(ap); \
	} \
	ret;\
})


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

static struct ut_list_head ut_mem_alloc_list = UT_LIST_HEAD_INIT(ut_mem_alloc_list);
int ut_mem_error()
{
	struct ut_list_head *node, *next;
	int err = 0;

	ut_list_for_each_safe(node, next, &ut_mem_alloc_list) {
		struct ut_mem_node *mem_node = container_of(node, struct ut_mem_node, list);
		void *p = mem_node + 1;
		
		if (mem_node->oob_data)
			free(mem_node->oob_data);

		if (!(UT_MEM_FLAG_NOCHECK & mem_node->flags)) {
			err++;
			/* resource leak */
			ut_printf("RES-LEAK[%d] %p~%p %5x %s\n", err, p, p + mem_node->size, mem_node->size, mem_node->trace_info);
		}

		ut_list_del(&(mem_node->list));
		free(mem_node);
	}
	
	ut_init_list_head(&ut_mem_alloc_list);
	
	return err;
}

void *ut_mem_oob_data(void *p)
{
	struct ut_mem_node *node = ut_mem_ptr_to_node(p);

	return node->oob_data;
}

void *ut_malloc_trace(int size, int oob_size, int flags, 
						const char *file, int line, const char *fmt, ...)
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
		ut_list_add(&(node->list), &ut_mem_alloc_list);

		free_len = sizeof(node->trace_info);
		buf = node->trace_info;
		ret = snprintf(buf, free_len, "%s:%i ", file?file:"??", line);
		if (ret && fmt) {
			free_len -= ret;
			buf += ret;
			if (free_len > 0) {
				va_list ap;
				va_start(ap, fmt);
				ret = vsnprintf(buf, free_len, fmt, ap);
				va_end(ap);
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

void ut_free_trace(void *p, const char *file, int line, const char *fmt, ...)
{
	struct ut_mem_node *mem_node;

	if (!p)
		return;

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

int ut_map_clear(struct ut_map *m)
{
	struct ut_list_head *node, *next;
	int cnt = 0;
	
	ut_list_for_each_safe(node, next, &(m->entrys)) {
		struct ut_map_node * e_node = container_of(node, struct ut_map_node, list);

		cnt++;

		ut_printf("MAP %s:[%x-%x]\n", m->name, e_node->e.key, e_node->e.data);
		ut_list_del(&(e_node->list));
		ut_free_trace(e_node, __FILE__, __LINE__, NULL);
	}
	
	ut_init_list_head(&(m->entrys));
	
	return cnt;
}


struct ut_entry *ut_map_find(struct ut_map *m, struct ut_entry *item)
{
	struct ut_list_head *node, *next;

	ut_list_for_each_safe(node, next, &(m->entrys)) {
		struct ut_map_node * e_node = container_of(node, struct ut_map_node, list);
		if (item->key == e_node->e.key) {
			return &(e_node->e);
		}
	}

	return NULL;
}

struct ut_entry *ut_map_insert(struct ut_map *m, const struct ut_entry *item)
{
	struct ut_map_node *e_node = NULL;

	e_node = ut_malloc_trace(sizeof(struct ut_map_node), 0, 0, 
		__FILE__, __LINE__, "MAP %s:[%x-%x]", m->name, item->key, item->data);
	
	if (e_node) {
		e_node->e.key = item->key;
		e_node->e.data = item->data;
		ut_list_add(&(e_node->list), &(m->entrys));
		return &(e_node->e);
	}

	return NULL;
}


struct ut_entry *ut_map_erase(struct ut_map *m, struct ut_entry *item)
{
	struct ut_entry *find = ut_map_find(m, item);
	
	if (find) {
		struct ut_map_node *e_node = container_of(find, struct ut_map_node, e);

		item->data = find->data;
		
		ut_list_del(&(e_node->list));
		ut_free_trace(e_node, __FILE__, __LINE__, NULL);

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

void _ut_cnt_range_append(struct ut_cnt_range *counter_range, int start, int end, char * file, int line)
{
	struct ut_range_node *range = ut_malloc_trace(sizeof(struct ut_range_node), 0, 0,
				file, line, "Trigger[%d:%d]", start, end);

	if (range) {
		int min = (start < end) ? start : end;
		int max = (start > end) ? start : end;
		range->base = min;
		range->size = (max - min) + 1;
		ut_list_add(&(range->list), &(counter_range->ranges));
	}
}

int _ut_cnt_range_check(struct ut_cnt_range *counter_range, int step)
{
	struct ut_list_head *node, *next;
	int val;
	int once_flag = 0;
	
	ut_list_for_each_safe(node, next, &(counter_range->ranges)) {
		struct ut_range_node *range = container_of(node, struct ut_range_node, list);

		if (0 == once_flag) {
			val = counter_range->counter;
			counter_range->counter += step;
			once_flag = 1;
		}
		
		if (range->size > 0)
			if (((range->base + range->size) >= val) && (val >= range->base)) {
				struct ut_mem_node *mem_node = ut_mem_ptr_to_node(range);
				ut_printf("ACTIVE COUNTER %s => %d %s\n", counter_range->name, val, mem_node->trace_info);
				return 1;
			}
	}

	return 0;
}

void _ut_cnt_range_clear(struct ut_cnt_range *counter_range, char * file, int line)
{
	struct ut_list_head *node, *next;

	ut_list_for_each_safe(node, next, &(counter_range->ranges)) {
		struct ut_range_node *range = container_of(node, struct ut_range_node, list);

		ut_list_del(&(range->list));
		ut_free_trace(range, file, line, NULL);
	}
	counter_range->counter = 0;
	ut_init_list_head(&(counter_range->ranges));
}

/* unit test common */
int ut_printf(const char *fmt, ...)
{
	return _ut_print_va_list(fmt);
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

void _ut_dumpstack(void) {
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
	int check_type = UT_CK_TYPE(flag);
	
	if (!cond) {
		char *name;
		int check_op = UT_CK_OP(flag);
		
		//KKSetResult(TC_FAIL);
		
		switch (check_type) {
		case _UT_CK_TYPE_EXPECT:
			name = "EXPECT";
			break;
		case _UT_CK_TYPE_ASSERT:
			name = "ASSERT";
			break;
		case _UT_CK_TYPE_STUB_ACTIVE:
			name = "STUB ACTIVE";
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
		ut_printf("[%s FAIL] at %s:%i: ", name, file, line);
#endif
		_ut_print_va_list(fmt);

		switch (check_op) {
		case _UT_CK_FAIL_EXIT:
#ifdef UT_DUMPSTACK
			_ut_dumpstack();
#endif
			exit(-1);
			break;
		}
	} else {
		//if (_UT_CK_TYPE_EXPECT == check_type)
			//KKSetResult(TC_PASS);
	}
	
	return cond;
}


#ifdef UT_TRACE_STUB
#define _ut_stub_register_info(act, handlename, currentname, newstub_name)	do { \
		ut_printf("[STUB %s] %s => %s%s%s\n", act, handlename, currentname, \
			newstub_name ? " => " : "", newstub_name ? newstub_name : "");\
	} while(0)
#define _ut_stub_unregister_info(act, handlname, currentname, newstub_name)	do { \
		ut_printf("[STUB %s] %s <= %s%s%s\n", act, handlname, currentname, \
			newstub_name ? " <= " : "", newstub_name ? newstub_name : "");\
	} while(0)
#else
#define _ut_stub_register_info(act, handlename, currentname, newstub_name) do{}while(0)
#define _ut_stub_unregister_info(act, handlname, currentname, newstub_name) do{}while(0)
#endif


int _ut_stub_register(struct ut_stub_handle *stub_handle, struct ut_stub_data *newstub, char * f, int line)
{
	int ret = -1;
	
	if (stub_handle->func && newstub) {
		struct ut_stub_data *stub = &stub_handle->stub;

		if (stub->type != newstub->type) {
			ut_printf("Stub error: %s:%i %s(%d) %s(%d) from %s:%i\n", __FUNCTION__, __LINE__,
				stub->name, stub->type, newstub->name, stub->type, f, line);
			exit(0);
		}
		
		if (UT_STUB_TYPE_ENTRY == stub->type) {
			//if (stub_handle->state)
				//(void)KKUninstallStub(stub_handle->func);
			//ret = KKInstallStub(stub_handle->func, newstub->val.func, STUB_TYPE_USER);
		} else if (UT_STUB_TYPE_RETURN == stub->type) {
			if (stub_handle->state) {
				//ret = KKSetStubULData(stub_handle->func, (unsigned long)(newstub->val.ret));
			} else {
				//ret = KKInstallULDataStub(stub_handle->func, (unsigned long)(newstub->val.ret));
			}
		} else
			ret = 0;
		
		if (0 == ret) {
			/* error */
			if (stub_handle->state)
				(void)_ut_fail_unless(0, UT_CK_FLAG(_UT_CK_FAIL_NOP, _UT_CK_TYPE_STUB_UPDATE), f, line,
					" %s => %s => %s\n", stub_handle->name, stub->name, newstub->name);
			else
				(void)_ut_fail_unless(0, UT_CK_FLAG(_UT_CK_FAIL_NOP, _UT_CK_TYPE_STUB_ACTIVE), f, line,
					" %s => %s\n", stub_handle->name, newstub->name);
		} else {
			/* successful */
			if (stub_handle->state)
				_ut_stub_register_info("===", stub_handle->name, stub->name, newstub->name);
			else
				_ut_stub_register_info(">>>", stub_handle->name, newstub->name, NULL);
			
			if (stub != newstub)
				*stub = *newstub;
			stub_handle->state = 1;
		}
	}

	if (0 == ret)
		return -1;
	else
		return 0;
}

void _ut_stub_unregister(struct ut_stub_handle *stub_handle, char * f, int line)
{
	if (stub_handle->func && stub_handle->state) {
		int ret;
		struct ut_stub_data *stub = &stub_handle->stub;
		
		//if (UT_STUB_TYPE_ENTRY == stub->type)
			//ret = KKUninstallStub(stub_handle->func);
		//else if (UT_STUB_TYPE_RETURN == stub->type)
			//ret = KKUninstallStub(stub_handle->func);
		//else
			ret = 0;
		
		if (0 == ret)
			(void)_ut_fail_unless(0, UT_CK_FLAG(_UT_CK_FAIL_NOP, _UT_CK_TYPE_STUB_DEACTIVE), f, line,
				" %s <= %s\n", stub_handle->name, stub->name);
		else
			_ut_stub_unregister_info("<<<<", stub_handle->name, stub->name, NULL);

		stub_handle->state = 0;
	}
}


