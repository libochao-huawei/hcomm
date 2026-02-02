#include <linux/percpu-defs.h>
#include <linux/preempt.h>
#include <linux/sched.h>

struct task_struct init_task;

//DEFINE_PER_CPU(int, __preempt_count) = 0;
//EXPORT_PER_CPU_SYMBOL(__preempt_count);

DEFINE_PER_CPU(struct task_struct *, current_task) = &init_task;
EXPORT_PER_CPU_SYMBOL(current_task);
