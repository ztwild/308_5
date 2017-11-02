/*
 * pset.c : contains the core implementation of HP processor sets for Linux 
 *
 * In a nutshell, processor sets are collections of 0 or more CPUs, a software
 * partition of your system into specialized sub-machines.
 * A task or a CPU belongs to exactly one processor set at a time.
 * Each pset will only run realtime jobs, kernel threads, and tasks 
 * bound to that pset on CPUs assigned to that pset.  If there are no CPUs
 * assigned, tasks in that pset are effectively frozen.  Scheduling in each 
 * processor set appears independent from user space. 
 *
 * The default processor set (0) always exists and may not be destroyed.
 * All tasks and processors start out in the system default group, including
 * new logins and daemons like the swapper.  For this reason, the last CPU may 
 * never be removed from the default group. (Hence, this feature is of little 
 * value on a single CPU system.)
 *
 * Details of the ioctl interface are provided in user-space documents for the 
 * pset library.
 *
 * Future versions should add support for more attributes.
 *
 * COPYRIGHT: HEWLETT-PACKARD CO. 2000
 * released under GNU GENERAL PUBLIC LICENSE
 * AUTHOR : Scott Rhine  (prmfeedback@rsn.hp.com)
 * HISTORY:
 * 2000-08-01   initial creation.
 * 2000-09-01   port to Linux 4.2
 * 2000-09-14   merged previous versions using ifdefs
 * 2000-09-26   eliminated assign of CPU 0 limitation
 * 2000-10-25   port to linux 2_4 test 9. fix initial best choice
 * 2001-01-05   use idtype_t from standard location for multi OS compat
 * 2001-01-24   port to linux 2_4_0, added choose_cpu
 * 2001-02-15   deleted unnecessary PSET_GETDFLTPSET
 * 2001-07-16   converted to cpus_allowed implementation (2.4.4)
 */

#define PSET_VERSION "pset 2.0"  /* update this every patch or release */

#include <linux/module.h> /* dynamic modules */
#include <linux/init.h>   /* init macro */

#include <linux/smp.h>
#include <linux/sched.h>  
#include <linux/altpolicy.h>  
#include <linux/smp_lock.h>
#include <linux/malloc.h>
#include <linux/fs.h>     /* file_operations */
#include <asm/uaccess.h>  /* copy_to/from_user */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/kmod.h>

#include "pset.h"

/* internal structures */
typedef struct processor_set_priv {
	psetid_t        ps_id;           /* created entity unique id */
        int             ps_spu_count;    /* number of cpus in this set */
	unsigned long   ps_cpus_allowed; /* vector shared by all processes */
        pset_attrval_t  ps_non_empty_op; /* behavior on delete of set in use */
        struct sched_policy* ps_next;    /* next pset in sorted list */
        struct sched_policy* ps_prev;    /* previous pset in sorted list */
} processor_set_priv_t;

#define PSET_PRIV(x) ((processor_set_priv_t *)(&(x)->sp_private))

static struct task_struct * pset_choose_task(struct task_struct *, int);
static struct sched_policy *default_pset = NULL;
static int psets_active = 0;
static rwlock_t psetlist_lock = RW_LOCK_UNLOCKED;
static psetid_t max_ps_id = PS_MAXPSET;
static struct sched_policy *pset_of_cpu[NR_CPUS];

static int pset_ioctl(struct inode *, struct file *, unsigned int, unsigned long);

struct file_operations pset_fops = {
	ioctl:		pset_ioctl
};

#define use_default_sched(p) \
	((p->mm == &init_mm) || (p->policy != SCHED_OTHER) || (!p->alt_policy))

#define is_visible(p,cpu) ((p)->cpus_allowed & (1 << cpu))

#ifdef __SMP__
#ifndef CONFIG_SMP
#define CONFIG_SMP
#endif
#endif

#ifdef CONFIG_SMP
#define can_choose(p, this_cpu) \
	((!p->has_cpu) && is_visible(p, this_cpu))
#else
#define can_choose(p, this_cpu) is_visible(p, this_cpu)
#endif

#ifdef MODULE
static int pset_busy = 0;
#endif

/* common macro for all priviledged operations */
#define PERMITTED()  capable(CAP_SYS_ADMIN)

/*------------------------------*
 *  scheduler-visible routines  *
 *------------------------------*/

static struct task_struct *
pset_choose_task(struct task_struct *curr_task, int this_cpu)
{
        int high;
        struct task_struct *p, *choice, *idle = idle_task(this_cpu);
	struct list_head *tmp;

	/*
	 * choose the most eligible task in this pset or the realtime pool.
         * note that the runqueue lock is held throughout the routine.
         * this is okay, because we loop a maximum of 3 times over the
         * run queue (recompute case).  This also prevents multiple CPUs
	 * from doing the same recompute.
	 */

recheck:
	high = IDLE_WEIGHT;
	choice = idle;

	/* with the advent of 2.4 test 9, SCHED_YIELD is no longer handled */
	if ((curr_task->state == TASK_RUNNING) && is_visible(curr_task, this_cpu)){
		high = goodness(curr_task, this_cpu, curr_task->active_mm);
		if (high >= 0)
			choice = curr_task;
	}

	/* pick the most runnable task in pset of this cpu */
	list_for_each(tmp, &runqueue_head)
	{
		p = list_entry(tmp, struct task_struct, run_list);
		if (can_choose(p, this_cpu)) {
			int w = goodness(p, this_cpu, curr_task->active_mm);
			if (w > high){
				high = w;
				choice = p;
			}
		}
	}

	/* if we had runnables with no ticks left, replenish counters */
	if (!high){
		list_for_each(tmp, &runqueue_head)
		{
			p = list_entry(tmp, struct task_struct, run_list);
			if (is_visible(p, this_cpu))
				p->counter = NICE_TO_TICKS(p->nice);
		}
		goto recheck;
	}

	return choice;
}

/*-------------------------------*
 *  load/unload module routines  *
 *-------------------------------*/

int __init
pset_init(void)
{
	int i, limit, rc = 0;
        struct task_struct *p;
	struct sched_policy *new;
	/* initialization of feature, happens once only */
	if (psets_active > 0)
		return 0;

	/* build system pset */

        new = kmalloc(sizeof(struct sched_policy)+sizeof(processor_set_priv_t), GFP_KERNEL);
        if (!new)
                return -ENOMEM;

	memset(new,0, sizeof(struct sched_policy)+sizeof(processor_set_priv_t));
	new->sp_choose_task = pset_choose_task;

	PSET_PRIV(new)->ps_id = PS_DEFAULT;
	PSET_PRIV(new)->ps_non_empty_op = PSET_ATTRVAL_FAILBUSY;
	PSET_PRIV(new)->ps_spu_count = NR_CPUS;
        PSET_PRIV(new)->ps_cpus_allowed = 0;

	default_pset = new;

	/* build per-cpu policy */
#ifdef CONFIG_SMP
	limit = smp_num_cpus;
#else
	limit = 1;
#endif

        for (i = 0; i < limit; i++) {
                int cpu;
		cpu = cpu_logical_map(i);
		pset_of_cpu[cpu] = default_pset;
                PSET_PRIV(new)->ps_cpus_allowed |= (1 << cpu);
	}

	if (register_sched (PSET_VERSION, pset_of_cpu, &pset_fops)){
		printk("pset: unable to register\n");
		return -EIO;
	}

	/* initialize all processes */
        read_lock(&tasklist_lock);
        for_each_task(p){
               	p->alt_policy = default_pset;
	}
        read_unlock(&tasklist_lock);

	PSET_PRIV(default_pset)->ps_spu_count = limit;
	psets_active = 1;

	return rc;
}

#ifdef MODULE
int
init_module(void)
{
	return pset_init( );
}

void
cleanup_module(void)
{
	/* prevent new creations while we deassemble the module */
	write_lock_irq(&psetlist_lock);

	unregister_sched(PSET_VERSION);
	kfree (default_pset);
	default_pset = NULL;
	psets_active = 0;

	write_unlock_irq(&psetlist_lock);
}
#endif

/*-----------------------------------*
 *  implementation of ioctl commands *
 *-----------------------------------*/

static int 
pset_create ( psetid_t *pset )
{
	/* create a new pset and return the id or an error */

        int retval = 0;
	struct sched_policy *new, *next, *curr_pset;

	if (!PERMITTED())
                return -EPERM;

	if (!pset)
		return -EFAULT;

	if (psets_active + 1 > max_ps_id)
                return -ENOMEM;

	/* allocate both parts together */
        new = kmalloc(sizeof(struct sched_policy)+sizeof(processor_set_priv_t), GFP_KERNEL);
        if (!new)
                return -ENOMEM;

	memset(new,0, sizeof(struct sched_policy)+sizeof(processor_set_priv_t));
	new->sp_choose_task = default_pset->sp_choose_task;
	new->sp_preemptability = default_pset->sp_preemptability;

        write_lock_irq(&psetlist_lock);

	curr_pset = default_pset;
	next = PSET_PRIV(curr_pset)->ps_next;

	/* look for a numbering gap */
	while (next){
		if (PSET_PRIV(next)->ps_id - PSET_PRIV(curr_pset)->ps_id > 1){
			PSET_PRIV(next)->ps_prev = new;
			break;
		}
		curr_pset = next;
		next = PSET_PRIV(curr_pset)->ps_next;
	}

	/* initialize private */
	PSET_PRIV(new)->ps_id = PSET_PRIV(curr_pset)->ps_id + 1;
	PSET_PRIV(new)->ps_non_empty_op = PSET_ATTRVAL_DFLTPSET;
	PSET_PRIV(new)->ps_spu_count = 0;

	/* insert into sorted list */
	PSET_PRIV(new)->ps_next = next;
	PSET_PRIV(new)->ps_prev = curr_pset;
	PSET_PRIV(curr_pset)->ps_next = new;

	*pset = PSET_PRIV(new)->ps_id;
	psets_active++;

#ifdef MODULE
	/* keep the dynamic module locked down when busy */
	if (!pset_busy){
		MOD_INC_USE_COUNT;
		pset_busy = 1;
	}
#endif
	write_unlock_irq(&psetlist_lock);
	return retval;
}

static int
pset_destroy ( psetid_t pset )
{
	/* 
         * destroy an existing pset.  if empty, follow special rules based on
         * the pset attributes
         */

	int retval = 0;
	struct sched_policy *curr_pset;
        struct task_struct *p;
        unsigned long belongs;	 /* cpus_allowed bit vector */

	if (!PERMITTED())
                return -EPERM;

	/* prevents destroy of 0 */
	if ((pset <= PS_DEFAULT) || (pset >= max_ps_id))
		return -EINVAL;

        write_lock_irq(&psetlist_lock);
	curr_pset = default_pset;

	/* look for a specific id */
	while (curr_pset){
		if (PSET_PRIV(curr_pset)->ps_id == pset)
			break;
		curr_pset = PSET_PRIV(curr_pset)->ps_next;
	}

	/* defaults to ESRCH if not found */

	if (curr_pset){
		belongs = PSET_PRIV(default_pset)->ps_cpus_allowed |
			  PSET_PRIV(curr_pset)->ps_cpus_allowed;

		if (PSET_PRIV(curr_pset)->ps_non_empty_op == PSET_ATTRVAL_FAILBUSY){

		        /* we must check this before can destroy */
        		read_lock(&tasklist_lock);
        		for_each_task(p){
               			if (p->alt_policy == curr_pset){
					retval = -EBUSY;
					break;
				}
			}
        		read_unlock(&tasklist_lock);

			if (retval){
				/* don't remove if process move failed */
				write_unlock_irq(&psetlist_lock);
				return retval;
			}

			/* set cpus_allowed for default group */
			if (PSET_PRIV(curr_pset)->ps_spu_count > 0){
        			read_lock(&tasklist_lock);
        			for_each_task(p){
               				if (p->alt_policy == default_pset)
						p->cpus_allowed = belongs;
				}
                		read_unlock(&tasklist_lock);
			}
		} else {
			int kill = 0;

			if (PSET_PRIV(curr_pset)->ps_non_empty_op == PSET_ATTRVAL_KILL)
				kill = 1;

                	read_lock(&tasklist_lock);

			/* move all tasks in destroyed group to default.
			 * set new cpus_allowed on all members of default.
			 */
                	for_each_task(p){
                        	if (p->alt_policy == curr_pset){
                                	p->alt_policy = default_pset;
					p->cpus_allowed = belongs;

                                	if (kill &&
					    (p->pid > 1) && (p->mm != &init_mm))
                                        	force_sig(SIGKILL, p);
				} else if (p->alt_policy == default_pset)
                                         p->cpus_allowed = belongs;
                        }
                	read_unlock(&tasklist_lock);
		}


#ifdef CONFIG_SMP
		/* adjust cpu to pset mappings */
		if (PSET_PRIV(curr_pset)->ps_spu_count > 0){
			int i, cpu;
			PSET_PRIV(default_pset)->ps_spu_count += PSET_PRIV(curr_pset)->ps_spu_count;
			PSET_PRIV(default_pset)->ps_cpus_allowed |=
					PSET_PRIV(curr_pset)->ps_cpus_allowed;

        		for (i = 0; i < smp_num_cpus; i++) {
				cpu = cpu_logical_map(i);
				if (pset_of_cpu[cpu] == curr_pset)
					pset_of_cpu[cpu]= default_pset;
			}
		}
#endif
		/* remove from pset list */

		if (PSET_PRIV(curr_pset)->ps_next)
			PSET_PRIV(PSET_PRIV(curr_pset)->ps_next)->ps_prev = PSET_PRIV(curr_pset)->ps_prev;
		if (PSET_PRIV(curr_pset)->ps_prev)
			PSET_PRIV(PSET_PRIV(curr_pset)->ps_prev)->ps_next = PSET_PRIV(curr_pset)->ps_next;
		PSET_PRIV(curr_pset)->ps_next = PSET_PRIV(curr_pset)->ps_prev = NULL; /* pedantic */
		kfree (curr_pset);
		psets_active--;
#ifdef MODULE
		if (pset_busy && (psets_active == 1)){
			MOD_DEC_USE_COUNT;
			pset_busy = 0;
		} 
#endif
	} else retval = -ESRCH;

	write_unlock_irq(&psetlist_lock);
	return retval;
}

static inline struct sched_policy *
find_pset(psetid_t ps_id)
{
	/* 
	 * converts pset id to the matching structure pointer. 
         * psetlist_lock must be locked 
         */
	struct sched_policy *curr_pset = default_pset;
	while (curr_pset){
		if (PSET_PRIV(curr_pset)->ps_id == ps_id)
			break;
		curr_pset = PSET_PRIV(curr_pset)->ps_next;
	}
	return curr_pset;
}

static inline int
find_spu(int id, struct sched_policy *pset_ptr)
{
#ifdef CONFIG_SMP
	int i, cpu, choice = INT_MAX;
	/* 
	 * given a pset, and a previous CPU in that set, return the next
         * sequentially ordered logical CPU number in the set.
         * psetlist_lock should be locked to avoid changes to pset_of_cpu.
	 * EAGAIN means there are no more processors left, don't call again.
         */
        for (i = 0; i < smp_num_cpus; i++) {
		cpu = cpu_logical_map(i);
		if ((cpu > id) && (cpu < choice) &&
		   (pset_of_cpu[cpu] == pset_ptr))
			choice = cpu;
	}
	if (choice < INT_MAX)
		return choice;
#else
	if ((id <0) && (pset_of_cpu[0] == pset_ptr))
		return 0;
#endif
	return -EAGAIN;
}

static int 
pset_assign ( psetid_t pset, int spu, psetid_t* opset)
{
	/* put a CPU into a pset, and return the old membership/error */

#ifdef CONFIG_SMP
	struct sched_policy *curr_pset;
	int retval = 0, i;
#endif

	if ((pset != PS_QUERY) && !PERMITTED())
                return -EPERM;

	if ((pset < PS_QUERY) || (pset > max_ps_id))
		return -EINVAL;

	if ((spu < 0) || (spu >= NR_CPUS))
		return -EINVAL;

#ifdef CONFIG_SMP
        for (i = 0; i < smp_num_cpus; i++) {
		if (spu == cpu_logical_map(i))
			break;
	}

	if (i >= smp_num_cpus)
		return -EINVAL;

	/* special case, just asking what current assignment is */
	if (pset == PS_QUERY){
		*opset = PSET_PRIV(pset_of_cpu[spu])->ps_id;
		return 0;
	}

	/* this prevents last cpu from ever leaving the default group */
	if ((PSET_PRIV(pset_of_cpu[spu])->ps_id == PS_DEFAULT) &&
	    (PSET_PRIV(pset_of_cpu[spu])->ps_spu_count <= 1))
		return -EBUSY;

       	write_lock_irq(&psetlist_lock);
	curr_pset = find_pset(pset);
	if (curr_pset){
		*opset = PSET_PRIV(pset_of_cpu[spu])->ps_id;
		
		if (curr_pset != pset_of_cpu[spu]){
                        unsigned long mask = 1 << spu ;
                        struct sched_policy *old_ptr;
                        unsigned long new_allowed, old_allowed;
                        struct task_struct *p;

			/* do the accounting */
			old_ptr = pset_of_cpu[spu];
                        PSET_PRIV(old_ptr)->ps_spu_count--;
                        PSET_PRIV(old_ptr)->ps_cpus_allowed &= (~mask);
                        old_allowed = PSET_PRIV(old_ptr)->ps_cpus_allowed;

			pset_of_cpu[spu] = curr_pset;

			PSET_PRIV(curr_pset)->ps_spu_count++;
			PSET_PRIV(curr_pset)->ps_cpus_allowed |= mask;
                        new_allowed = PSET_PRIV(curr_pset)->ps_cpus_allowed;

                        /* reset bit vectors of every proc in new + old pset */
                        read_lock(&tasklist_lock);
                        for_each_task(p){
                                if ( p->alt_policy == curr_pset )
                                        p->cpus_allowed = new_allowed;
                                else if (p->alt_policy == old_ptr)
                                        p->cpus_allowed = old_allowed;
                        }
                        read_unlock(&tasklist_lock);

                        /* kick off the currently running job */
			resched_cpu(spu);
		}
	} else retval = -ESRCH;
       	write_unlock_irq(&psetlist_lock);

	return retval;
#else
	/* special case, just asking what current assignment is */
	if (pset == PS_QUERY){
		*opset = PS_DEFAULT;
		return 0;
	}
	return -EBUSY;
#endif
}

static inline int
pset_move(struct task_struct *p, struct sched_policy *newpolicy)
{
	/* 
	 * called only by pset_bind.
 	 * the move operation can be tricky.  
 	 * retval = 0 clears the error condition if we succeed at least once.  
 	 * if you transition to an alien pset,
 	 * you must forfeit all rights to time on the current set.  
 	 * if the new set has no member cpus, you effectively freeze.
 	 */
	if (p->alt_policy != newpolicy){
        	p->alt_policy = newpolicy; /* must come before resched! */
#ifdef CONFIG_SMP
 		p->cpus_allowed = PSET_PRIV(newpolicy)->ps_cpus_allowed;

		if (p->has_cpu) 
#else
		if (p == current)
#endif
			resched_cpu(p->processor);
		if (!PSET_PRIV(newpolicy)->ps_spu_count)
			p->counter = 0;	
	}
        return 0;
}

static int 
pset_bind ( psetid_t pset, idtype_t idtype, id_t id, psetid_t *opset)
{
	/* put a task into a pset, and return the old membership/error */

	struct sched_policy *curr_pset;
	int retval = -ESRCH;
        struct task_struct *p;

	if (pset == PS_QUERY){
		if (id == PS_MYID){
			*opset = PSET_PRIV(current->alt_policy)->ps_id;
			retval = 0;
		} else {
			read_lock(&tasklist_lock);
			p = find_task_by_pid(id);
			if (p){
				*opset = PSET_PRIV(p->alt_policy)->ps_id;
				retval = 0;
			}
			read_unlock(&tasklist_lock);
		}
		return retval;
        }

	/* note: we ignore opset for all but PID type */

	if (!PERMITTED())
                return -EPERM;

	if ((pset < PS_DEFAULT) || (pset > max_ps_id))
		return -EINVAL;

	if (id < PS_MYID)
		return -EINVAL;

	switch(idtype){
		case P_PGID:
			if (id == PS_MYID)
				id = current->pgrp;
			if ((id == 0) || (id == 1))
				return -EPERM;
			break;
		case P_PID:
			if (id == PS_MYID)
				id = current->pid;
			break;
		default:
			/* LATER, implement THREAD and UID */
			return -EINVAL;
	}

        read_lock(&psetlist_lock);
        curr_pset = find_pset(pset);

	/* default is pset or task not found */
        if (curr_pset){
        	read_lock(&tasklist_lock);
		switch(idtype){
		case P_PGID:
        		for_each_task(p){
				if (p->pgrp == id)
					retval = pset_move (p, curr_pset);
			}
			break;
		default:
		case P_PID:
        		for_each_task(p){
				if (p->pid == id){
					*opset = PSET_PRIV(p->alt_policy)->ps_id;
					retval = pset_move (p, curr_pset);
				}
			}
			break;
		}
        	read_unlock(&tasklist_lock);
        }
        read_unlock(&psetlist_lock);

	return retval;
}

static int 
pset_getattr ( psetid_t pset, pset_attrtype_t type, pset_attrval_t* value)
{
	/* look up the specified attribute of the indicated pset */

	struct sched_policy *curr_pset;
	int retval = 0;

	if ((pset < PS_DEFAULT) || (pset >= max_ps_id))
		return -EINVAL;

	if (!value)
		return -EFAULT;

	if (type != PSET_ATTR_NONEMPTY)
		return -EINVAL;

       	read_lock(&psetlist_lock);
	curr_pset = find_pset(pset);
	if (curr_pset)
		*value = PSET_PRIV(curr_pset)->ps_non_empty_op;
	else retval = -ESRCH;
       	read_unlock(&psetlist_lock);

	return retval;
}

static int 
pset_setattr ( psetid_t pset, pset_attrtype_t type, pset_attrval_t value)
{
	/* set the specified attribute of the indicated pset */

	struct sched_policy *curr_pset;
	int retval = 0;

	if (!PERMITTED())
                return -EPERM;

	/* prevents change of 0 */
	if ((pset <= PS_DEFAULT) || (pset > max_ps_id))
		return -EINVAL;

	if (type != PSET_ATTR_NONEMPTY)
		return -EINVAL;

	if ((value < PSET_ATTRVAL_FAILBUSY) || (value > PSET_ATTRVAL_KILL))
		return -EINVAL;

       	read_lock(&psetlist_lock);
	curr_pset = find_pset(pset);
	if (curr_pset)
		PSET_PRIV(curr_pset)->ps_non_empty_op = value;
	else retval = -ESRCH;
       	read_unlock(&psetlist_lock);

	return retval;
}

static int
pset_ctl ( pset_request_t req, psetid_t pset, id_t id)
{
	/* look up a wide range of information about pset, CPU, or own task */

	int retval = -EINVAL;
	struct sched_policy *pset_ptr;

	switch(req){
        case PSET_GETNUMPSETS: 
		retval = psets_active;
		break;

        case PSET_GETFIRSTPSET:
		retval = PS_DEFAULT;
		break;

        case PSET_GETNEXTPSET:
       		read_lock(&psetlist_lock);
        	pset_ptr = default_pset;
        	while (pset_ptr){
                	if (PSET_PRIV(pset_ptr)->ps_id > pset)
                        	break;
                	pset_ptr = PSET_PRIV(pset_ptr)->ps_next;
        	}
		if (!pset_ptr)
			retval = -ESRCH;
		else retval = PSET_PRIV(pset_ptr)->ps_id;
       		read_unlock(&psetlist_lock);
		break;

        case PSET_GETCURRENTPSET:
		retval = -ESRCH;
		
       		read_lock(&psetlist_lock);
		retval = PSET_PRIV(current->alt_policy)->ps_id;
       		read_unlock(&psetlist_lock);
		break;

        case PSET_GETNUMSPUS:
       		read_lock(&psetlist_lock);
		pset_ptr = find_pset(pset);
		if (pset_ptr){
			retval = PSET_PRIV(pset_ptr)->ps_spu_count;
		} else retval = -ESRCH;
       		read_unlock(&psetlist_lock);
		break;
	
        case PSET_GETFIRSTSPU:
		id = -1;
		/* fall through to PSET_GETNEXTSPU */
        case PSET_GETNEXTSPU:
       		read_lock(&psetlist_lock);
		pset_ptr = find_pset(pset);
		if (!pset_ptr)
			retval = -ESRCH;
		else if (!PSET_PRIV(pset_ptr)->ps_spu_count)
			retval = -ENOENT;
                else retval = find_spu(id, pset_ptr); 

       		read_unlock(&psetlist_lock);
		break;
        case PSET_SPUTOPSET:
#ifdef CONFIG_SMP
		{
			int i;

        		for (i = 0; i < smp_num_cpus; i++) {
				if (id == cpu_logical_map(i))
					break;
			}

			if (i >= smp_num_cpus)
				retval =  -EINVAL;
			else {
       				read_lock(&psetlist_lock);
				pset_ptr = pset_of_cpu[id];
				if (!pset_ptr)
					retval = PS_DEFAULT;
				else retval = PSET_PRIV(pset_ptr)->ps_id;
       				read_unlock(&psetlist_lock);
			}
		}
#else
		if (id == 0)
			retval = PS_DEFAULT;
#endif
		break;
	default :
		/* defaults to EINVAL */
		break;
	}

	return retval;
}

static int 
pset_ioctl(struct inode * inode, struct file * file, unsigned int cmd, unsigned long arg)
{
	/* 
	 * the master interface. converts ioctl into internal method, then
	 * copies out answer or error code. 
	 */
	int retval;

	if (cmd > PSIOC_BASE)
		cmd -= PSIOC_BASE;

        switch (cmd) {
	case PSIOC_CREATE:
		{
		psetid_t output;

		retval = pset_create(&output);
		if (!retval){
			if (copy_to_user((psetid_t *)arg, &output, sizeof(psetid_t))){
				/* clean up on any failure */
				pset_destroy(retval);
				retval = -EFAULT;
			}
		}
		break;
		}

	case PSIOC_DESTROY:
		retval = pset_destroy(arg);
		break;

        case PSIOC_ASSIGN:
		{
		pset_assign_t tmp;
		psetid_t output;

                if (copy_from_user(&tmp, (pset_assign_t *)arg, sizeof(pset_assign_t)))
                	return -EFAULT;
		retval = pset_assign(tmp.pset, tmp.spu, &output);
		if (!retval){
			if (copy_to_user(tmp.opset, &output, sizeof(psetid_t)))
				retval = -EFAULT;
		}
		break;
		}

	case PSIOC_BIND:
		{
		pset_bind_t tmp;
		psetid_t output;

                if (copy_from_user(&tmp, (pset_bind_t *)arg, sizeof(pset_bind_t)))
                	return -EFAULT;
		retval = pset_bind(tmp.pset, tmp.idtype, tmp.id, &output);
		if (!retval){
			if (copy_to_user(tmp.opset, &output, sizeof(psetid_t)))
				retval = -EFAULT;
		}
		break;
		}

	case PSIOC_GETATTR:
		{
		pset_getattr_t tmp;
		pset_attrval_t output;

                if (copy_from_user(&tmp, (pset_getattr_t *)arg, sizeof(pset_getattr_t)))
                	return -EFAULT;
		retval = pset_getattr(tmp.pset, tmp.type, &output);
		if (!retval){
			if (copy_to_user(tmp.value, &output, sizeof(pset_attrval_t)))
				retval = -EFAULT;
		}
		break;
		}

	case PSIOC_SETATTR:
		{
		pset_setattr_t tmp;

                if (copy_from_user(&tmp, (pset_setattr_t *)arg, sizeof(pset_setattr_t)))
                	return -EFAULT;
		retval = pset_setattr(tmp.pset, tmp.type, tmp.value);
		break;
		}

	case PSIOC_CTL:
		{
		pset_ctl_t tmp;

                if (copy_from_user(&tmp, (pset_ctl_t *)arg, sizeof(pset_ctl_t)))
                	return -EFAULT;
		retval = pset_ctl(tmp.req, tmp.pset, tmp.id);
		break;
		}

	default:
		retval = -EINVAL;
		break;
	};
	return retval;
}
