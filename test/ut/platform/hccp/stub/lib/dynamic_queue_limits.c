#include "stdio.h"

#include <linux/kernel.h>
#include <linux/jiffies.h>

#include <linux/dynamic_queue_limits.h>

/* Records completed count and recalculates the queue limit */
void dql_completed(struct dql *dql, unsigned int count)
{
}
EXPORT_SYMBOL(dql_completed);

void dql_reset(struct dql *dql)
{
	/* Reset all dynamic values */
	dql->limit = 0;
	dql->num_queued = 0;
	dql->num_completed = 0;
	dql->last_obj_cnt = 0;
	dql->prev_num_queued = 0;
	dql->prev_last_obj_cnt = 0;
	dql->prev_ovlimit = 0;
	dql->lowest_slack = UINT_MAX;
	dql->slack_start_time = jiffies;
}
EXPORT_SYMBOL(dql_reset);