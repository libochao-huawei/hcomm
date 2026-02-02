/* CPU control.
 * (C) 2001, 2002, 2003, 2004 Rusty Russell
 *
 * This code is licenced under the GPL.
 */
#include <linux/proc_fs.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/notifier.h>
//#include <linux/sched/signal.h>
//#include <linux/sched/hotplug.h>
//#include <linux/sched/task.h>
#include <linux/unistd.h>
#include <linux/cpu.h>

enum system_states system_state = SYSTEM_RUNNING;

struct cpumask __cpu_online_mask __read_mostly;
EXPORT_SYMBOL(__cpu_online_mask);

int tc_num_online_cpus = 1;
