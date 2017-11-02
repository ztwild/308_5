38:#define PSET_VERSION "pset 2.0"  /* update this every patch or release */
39:
40:#include <linux/module.h> /* dynamic modules */
41:#include <linux/init.h>   /* init macro */
42:
43:#include <linux/smp.h>
44:#include <linux/sched.h>  
45:#include <linux/altpolicy.h>  
46:#include <linux/smp_lock.h>
47:#include <linux/malloc.h>
48:#include <linux/fs.h>     /* file_operations */
49:#include <asm/uaccess.h>  /* copy_to/from_user */
50:
51:#include <linux/config.h>
52:#include <linux/kernel.h>
53:#include <linux/kmod.h>
54:
55:#include "pset.h"
56:
57:/* internal structures */
58:typedef struct processor_set_priv {
59:	psetid_t	ps_id;	   /* created entity unique id */
60:	int	     ps_spu_count;    /* number of cpus in this set */
61:	unsigned long   ps_cpus_allowed; /* vector shared by all processes */
62:	pset_attrval_t  ps_non_empty_op; /* behavior on delete of set in use */
63:	struct sched_policy* ps_next;    /* next pset in sorted list */
64:	struct sched_policy* ps_prev;    /* previous pset in sorted list */
65:} processor_set_priv_t;
66:
67:#define PSET_PRIV(x) ((processor_set_priv_t *)(&(x)->sp_private))
68:
69:static struct task_struct * pset_choose_task(struct task_struct *, int);
70:static struct sched_policy *default_pset = NULL;
71:static int psets_active = 0;
72:static rwlock_t psetlist_lock = RW_LOCK_UNLOCKED;
73:static psetid_t max_ps_id = PS_MAXPSET;
74:static struct sched_policy *pset_of_cpu[NR_CPUS];
75:
76:static int pset_ioctl(struct inode *, struct file *, unsigned int, unsigned long);
77:
78:struct file_operations pset_fops = {
79:	ioctl:	  pset_ioctl
80:};
81:
82:#define use_default_sched(p) \
83:	((p->mm == &init_mm) || (p->policy != SCHED_OTHER) || (!p->alt_policy))
84:
85:#define is_visible(p,cpu) ((p)->cpus_allowed & (1 << cpu))
86:
87:#ifdef __SMP__
88:#ifndef CONFIG_SMP
89:#define CONFIG_SMP
90:#endif
91:#endif
92:
93:#ifdef CONFIG_SMP
94:#define can_choose(p, this_cpu) \
95:	((!p->has_cpu) && is_visible(p, this_cpu))
96:#else
97:#define can_choose(p, this_cpu) is_visible(p, this_cpu)
98:#endif
99:
100:#ifdef MODULE
101:static int pset_busy = 0;
102:#endif
103:
104:/* common macro for all priviledged operations */
105:#define PERMITTED()  capable(CAP_SYS_ADMIN)
106:
107:/*------------------------------*
108: *  scheduler-visible routines  *
109: *------------------------------*/
110:
111:static struct task_struct *
112:pset_choose_task(struct task_struct *curr_task, int this_cpu)
113:{
114:	int high;
115:	struct task_struct *p, *choice, *idle = idle_task(this_cpu);
116:	struct list_head *tmp;
117:
118:	/*
119:	 * choose the most eligible task in this pset or the realtime pool.
120:	 * note that the runqueue lock is held throughout the routine.
121:	 * this is okay, because we loop a maximum of 3 times over the
122:	 * run queue (recompute case).  This also prevents multiple CPUs
123:	 * from doing the same recompute.
124:	 */
125:
126:recheck:
127:	high = IDLE_WEIGHT;
128:	choice = idle;
129:
130:	/* with the advent of 2.4 test 9, SCHED_YIELD is no longer handled */
131:	if ((curr_task->state == TASK_RUNNING) && is_visible(curr_task, this_cpu)){
132:		high = goodness(curr_task, this_cpu, curr_task->active_mm);
133:		if (high >= 0)
134:			choice = curr_task;
135:	}
136:
137:	/* pick the most runnable task in pset of this cpu */
138:	list_for_each(tmp, &runqueue_head)
139:	{
140:		p = list_entry(tmp, struct task_struct, run_list);
141:		if (can_choose(p, this_cpu)) {
142:			int w = goodness(p, this_cpu, curr_task->active_mm);
143:			if (w > high){
144:				high = w;
145:				choice = p;
146:			}
147:		}
148:	}
149:
150:	/* if we had runnables with no ticks left, replenish counters */
151:	if (!high){
152:		list_for_each(tmp, &runqueue_head)
153:		{
154:			p = list_entry(tmp, struct task_struct, run_list);
155:			if (is_visible(p, this_cpu))
156:				p->counter = NICE_TO_TICKS(p->nice);
157:		}
158:		goto recheck;
159:	}
160:
161:	return choice;
162:}
163:
164:/*-------------------------------*
165: *  load/unload module routines  *
166: *-------------------------------*/
167:
168:int __init
169:pset_init(void)
170:{
171:	int i, limit, rc = 0;
172:	struct task_struct *p;
173:	struct sched_policy *new;
174:	/* initialization of feature, happens once only */
175:	if (psets_active > 0)
176:		return 0;
177:
178:	/* build system pset */
179:
180:	new = kmalloc(sizeof(struct sched_policy)+sizeof(processor_set_priv_t), GFP_KERNEL);
181:	if (!new)
182:		return -ENOMEM;
183:
184:	memset(new,0, sizeof(struct sched_policy)+sizeof(processor_set_priv_t));
185:	new->sp_choose_task = pset_choose_task;
186:
187:	PSET_PRIV(new)->ps_id = PS_DEFAULT;
188:	PSET_PRIV(new)->ps_non_empty_op = PSET_ATTRVAL_FAILBUSY;
189:	PSET_PRIV(new)->ps_spu_count = NR_CPUS;
190:	PSET_PRIV(new)->ps_cpus_allowed = 0;
191:
192:	default_pset = new;
193:
194:	/* build per-cpu policy */
195:#ifdef CONFIG_SMP
196:	limit = smp_num_cpus;
197:#else
198:	limit = 1;
199:#endif
200:
201:	for (i = 0; i < limit; i++) {
202:		int cpu;
203:		cpu = cpu_logical_map(i);
204:		pset_of_cpu[cpu] = default_pset;
205:		PSET_PRIV(new)->ps_cpus_allowed |= (1 << cpu);
206:	}
207:
208:	if (register_sched (PSET_VERSION, pset_of_cpu, &pset_fops)){
209:		printk("pset: unable to register\n");
210:		return -EIO;
211:	}
212:
213:	/* initialize all processes */
214:	read_lock(&tasklist_lock);
215:	for_each_task(p){
216:		p->alt_policy = default_pset;
217:	}
218:	read_unlock(&tasklist_lock);
219:
220:	PSET_PRIV(default_pset)->ps_spu_count = limit;
221:	psets_active = 1;
222:
223:	return rc;
224:}
225:
226:#ifdef MODULE
227:int
228:init_module(void)
229:{
230:	return pset_init( );
231:}
232:
233:void
234:cleanup_module(void)
235:{
236:	/* prevent new creations while we deassemble the module */
237:	write_lock_irq(&psetlist_lock);
238:
239:	unregister_sched(PSET_VERSION);
240:	kfree (default_pset);
241:	default_pset = NULL;
242:	psets_active = 0;
243:
244:	write_unlock_irq(&psetlist_lock);
245:}
246:#endif
247:

