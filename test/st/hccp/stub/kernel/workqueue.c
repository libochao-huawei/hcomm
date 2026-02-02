#include <linux/workqueue.h>


struct workqueue_struct *system_wq; 

static struct work_struct *stub_work_desc[1024];
u32 stub_work_id = 0;
typedef	void (*work_fn_t)(struct work_struct *work);

/*
* Return:
*  1	   if @work was pending and we successfully stole PENDING
*  0	   if @work was idle and we claimed PENDING
*/
bool queue_work_on(int cpu, struct workqueue_struct *wq,
			struct work_struct *work)
{
	int i;
	work_func_t func = NULL;

	if (work)
		func = work->func;

	if (func)
		func(work);
	
	return 1;
}

bool cancel_work_sync(struct work_struct *work)
{
	return 1;
}

void stub_init_work(struct work_struct *_work, work_fn_t _func)
{
	(_work)->func = (_func);
}

bool queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
			   struct delayed_work *dwork, unsigned long delay)
{
	struct work_struct *work = &dwork->work;
	work_func_t func = NULL;
	int i;

	if (work)
		func = work->func;

	if (func)
		func(work);

	return 1;
}

bool cancel_delayed_work_sync(struct delayed_work *dwork)
{
	return 1;
}

void delayed_work_timer_fn(unsigned long __data)
{
	return;
}

bool mod_delayed_work_on(int cpu, struct workqueue_struct *wq,
			 struct delayed_work *dwork, unsigned long delay)
{
	return true;
}
EXPORT_SYMBOL_GPL(mod_delayed_work_on);