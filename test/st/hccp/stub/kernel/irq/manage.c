#include "ut_lib.h"

#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
//#include <linux/sched/task.h>
//#include <uapi/linux/sched/types.h>
#include <linux/task_work.h>

#ifndef PRINTF
#define PRINTF(fmt, ...)	\
	printf("****************[Line: %04d. %s] " fmt,  __LINE__, __func__, ## __VA_ARGS__)
#endif

static struct irq_desc *stub_irq_desc[1024];
void free_irq(unsigned int irq, void *dev_id)
{
	struct irq_desc * desc;
	struct irqaction * action;
	void *name = NULL;

	if (irq > 1023) {
		//PRINTF("irq: %u\n", irq);
		irq = 0;
	}

	desc = stub_irq_desc[irq];
	if (!desc)
		return ;
	stub_irq_desc[irq] = NULL;
	
	action = (struct irqaction *)(desc + 1);
	name = action->name;
	
	ut_free(desc);

	return ;
}

int __request_threaded_irq(unsigned int irq, irq_handler_t handler,
			 irq_handler_t thread_fn, unsigned long irqflags,
			 const char *devname, void *dev_id, const char *file, int line)
{
	struct irq_desc * desc;
	struct irqaction * action;

	if (irq > 1023) {
		//PRINTF("irq: %u\n", irq);
		irq = 0;
	}

	if (stub_irq_desc[irq])
		return 0;

	desc = ut_malloc_trace(sizeof(*desc) + sizeof(*action), 0, 0,
			file, line, "IRQ-ALLOC:irq=%d,dev=%s", irq, devname);

	if (!desc)
		return -ENOMEM;
	
	stub_irq_desc[irq] = desc;
	
	memset(desc, 0, sizeof(*desc));
	action = (struct irqaction *)(desc + 1);

	desc->name = devname;
	desc->action = action;
	
	memset(action, 0, sizeof(*action));
	action->handler = handler;
	action->irq = irq;
	action->name = devname;
	action->dev_id = dev_id;
	
	return 0;
}

int stub_exec_irq_handler(int irq)
{
	struct irq_desc * desc;
	struct irqaction * action;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(stub_irq_desc); i++) {
		desc = stub_irq_desc[i];
		if (!desc)
			continue;
		
		action = (struct irqaction *)(desc + 1);
		if ((irq == action->irq) || (-1 == irq)) {
			if (action && action->handler) {
				action->handler(irq, action->dev_id);
			}
		}
	}

	return 0;
}

int stub_exec_all_irq_handler()
{
	return stub_exec_irq_handler(-1);
}



int irq_set_affinity_hint(unsigned int irq, const struct cpumask *m)
{

	return 0;
}
EXPORT_SYMBOL_GPL(irq_set_affinity_hint);



/**
 *	disable_irq - disable an irq and wait for completion
 *	@irq: Interrupt to disable
 *
 *	Disable the selected interrupt line.  Enables and Disables are
 *	nested.
 *	This function waits for any pending IRQ handlers for this interrupt
 *	to complete before returning. If you use this function while
 *	holding a resource the IRQ handler may need you will deadlock.
 *
 *	This function may be called - with care - from IRQ context.
 */
void disable_irq(unsigned int irq)
{

}
EXPORT_SYMBOL(disable_irq);
