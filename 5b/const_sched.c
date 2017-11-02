/*
 * const_sched.c : contains the core implementation of a minimalistic
 *		   constant time scheduler for Linux.  It ignores threads,
 *		   real-time, and nice values, assuming that all things are
 *		   equal. If this is your common case, you can avoid walking
 *		   the entire run-queue each time and just take the next
 *		   item on the queue.
 * 
 * performance quirk: for 2.2 SMP, you are guaranteed to thrash cache for small
 * 		   numbers of processes.  We dirty pointers for init, a
 *		   currently running job, and the next job to be run.
 *		   This cost is exceeded by queue traversal costs when number
 *		   of processes is over about 60 on a 4 CPU box.
 *		   Uniprocessor and 2.4 have no known problems.
 *
 * COPYRIGHT: HEWLETT-PACKARD CO. 2000
 * released under GNU GENERAL PUBLIC LICENSE
 * AUTHOR : Scott Rhine  (prmfeedback@rsn.hp.com)
 * HISTORY:
 * 2000-09-20   initial creation.
 * 2000-10-25   conversion to Linux 2_4 test 9
 * 2001-03-31   fixed uniprocessor/laptop hang under IO stress for 2_4 only
 */

/* update this every patch or release */
#define CTRR_SCHED_VERSION "constant time round robin scheduler 1.3"  

#include <linux/module.h> /* dynamic modules */
#include <linux/init.h>   /* init macro */

#include <linux/smp.h>
#include <linux/sched.h>  
#include <linux/altpolicy.h>  
#include <linux/smp_lock.h>
#include <linux/malloc.h>
#include <linux/fs.h>     /* file_operations */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/kmod.h>


#if (LINUX_VERSION_CODE >= 0x020400)
struct file_operations ctrr_fops = {
};
#else
struct file_operations ctrr_fops = { NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
#endif
static struct sched_policy *ctrr_of_cpu[NR_CPUS];
static struct sched_policy round_robin;

/*------------------------------*
 *  scheduler-visible routines  *
 *------------------------------*/

static struct task_struct * 
ctrr_choose_task(struct task_struct *curr_task, int this_cpu)
{
	register struct task_struct *p;
	int keep = 0;

#if (LINUX_VERSION_CODE >= 0x020400)
	struct list_head *tmp;

	/* short cut. just keep running current job if we can */
        if ((curr_task->state == TASK_RUNNING)&&
	    !(curr_task->policy & SCHED_YIELD)&&
	    (curr_task != idle_task(this_cpu)) &&
	    (curr_task->counter > 0)){
		keep = 1;
	} 

	list_for_each(tmp, &runqueue_head) {
		p = list_entry(tmp, struct task_struct, run_list);
#ifdef CONFIG_SMP
		if (p->has_cpu || (p->policy & SCHED_YIELD)){
			continue;
		}
#else 
		if (p->policy & SCHED_YIELD){
                        continue;
                }
#endif
		/* perform simple preempt test */
		if (keep && (curr_task->counter >= p->counter))
			return curr_task;

                /* remove from front of queue and stick back on the back */
		list_del(&p->run_list);
		list_add_tail(&p->run_list, &runqueue_head);

                p->counter = NICE_TO_TICKS(p->nice);
                return p;
        }

	/* nothing good found.  fall thru to current or idle */
	if (keep)
		return curr_task;

#else  /* pre 2.4 */
	register struct task_struct *next;
	register struct task_struct *prev;

        if (curr_task->state == TASK_RUNNING){
		if (curr_task->policy & SCHED_YIELD){
			curr_task->policy &= ~SCHED_YIELD;
		} else if (curr_task->counter > 0){
			/* short cut. just keep running current job if we can */
			return curr_task;
		}
	} 

	p = init_task.next_run;

#ifdef __SMP__
	/* skip over the ones already running */
	while (p->has_cpu && p != &init_task)
		p = p->next_run;
#endif

	if (p != &init_task){

		/* remove from front of queue and stick back on the back */
		next = p->next_run;
		prev = p->prev_run;
		next->prev_run = prev;
		prev->next_run = next;
		p->next_run = &init_task;
		prev = init_task.prev_run;
		init_task.prev_run = p;
		p->prev_run = prev;
		prev->next_run = p;
		
                p->counter = p->priority; 
		return p;
	}
#endif  /* end LINUX version */
	return idle_task(this_cpu);
}

static int 
ctrr_preemptability (struct task_struct *curr_task, struct task_struct * thief, int this_cpu)
{
	return (thief->counter - curr_task->counter);
}

/*-------------------------------*
 *  load/unload module routines  *
 *-------------------------------*/

#ifdef MODULE
int  __init
init_module(void)
{
	int i;
        struct task_struct *p;
	struct sched_policy *new = &round_robin;

	memset (new, 0, sizeof(struct sched_policy));
	new->sp_choose_task = ctrr_choose_task;
	new->sp_preemptability = ctrr_preemptability;

	/* build per-cpu policy */
        for (i = 0; i < NR_CPUS; i++)
		ctrr_of_cpu[i] = new;

	if (register_sched (CTRR_SCHED_VERSION, ctrr_of_cpu, &ctrr_fops)){
		printk("const_sched: unable to register\n");
		return -EIO;
	}

	/* initialize all processes */
        read_lock(&tasklist_lock);
        for_each_task(p){
               	p->alt_policy = new;
	}
        read_unlock(&tasklist_lock);
	return 0;
}

void
cleanup_module(void)
{
	unregister_sched(CTRR_SCHED_VERSION);
}
#endif
