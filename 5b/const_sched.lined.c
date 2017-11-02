25:/* update this every patch or release */
26:#define CTRR_SCHED_VERSION "constant time round robin scheduler 1.3"  
27:
28:#include <linux/module.h> /* dynamic modules */
29:#include <linux/init.h>   /* init macro */
30:
31:#include <linux/smp.h>
32:#include <linux/sched.h>  
33:#include <linux/altpolicy.h>  
34:#include <linux/smp_lock.h>
35:#include <linux/malloc.h>
36:#include <linux/fs.h>     /* file_operations */
37:
38:#include <linux/config.h>
39:#include <linux/kernel.h>
40:#include <linux/kmod.h>
41:
42:
43:#if (LINUX_VERSION_CODE >= 0x020400)
44:struct file_operations ctrr_fops = {
45:};
46:#else
47:struct file_operations ctrr_fops = { NULL, NULL, NULL, NULL, NULL, NULL, NULL,
48:	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
49:#endif
50:static struct sched_policy *ctrr_of_cpu[NR_CPUS];
51:static struct sched_policy round_robin;
52:
53:/*------------------------------*
54: *  scheduler-visible routines  *
55: *------------------------------*/
56:
57:static struct task_struct * 
58:ctrr_choose_task(struct task_struct *curr_task, int this_cpu)
59:{
60:	register struct task_struct *p;
61:	int keep = 0;
62:
63:#if (LINUX_VERSION_CODE >= 0x020400)
64:	struct list_head *tmp;
65:
66:	/* short cut. just keep running current job if we can */
67:	if ((curr_task->state == TASK_RUNNING)&&
68:	    !(curr_task->policy & SCHED_YIELD)&&
69:	    (curr_task != idle_task(this_cpu)) &&
70:	    (curr_task->counter > 0)){
71:		keep = 1;
72:	} 
73:
74:	list_for_each(tmp, &runqueue_head) {
75:		p = list_entry(tmp, struct task_struct, run_list);
76:#ifdef CONFIG_SMP
77:		if (p->has_cpu || (p->policy & SCHED_YIELD)){
78:			continue;
79:		}
80:#else 
81:		if (p->policy & SCHED_YIELD){
82:			continue;
83:		}
84:#endif
85:		/* perform simple preempt test */
86:		if (keep && (curr_task->counter >= p->counter))
87:			return curr_task;
88:
89:		/* remove from front of queue and stick back on the back */
90:		list_del(&p->run_list);
91:		list_add_tail(&p->run_list, &runqueue_head);
92:
93:		p->counter = NICE_TO_TICKS(p->nice);
94:		return p;
95:	}
96:
97:	/* nothing good found.  fall thru to current or idle */
98:	if (keep)
99:		return curr_task;
100:
101:#else  /* pre 2.4 */
102:	register struct task_struct *next;
103:	register struct task_struct *prev;
104:
105:	if (curr_task->state == TASK_RUNNING){
106:		if (curr_task->policy & SCHED_YIELD){
107:			curr_task->policy &= ~SCHED_YIELD;
108:		} else if (curr_task->counter > 0){
109:			/* short cut. just keep running current job if we can */
110:			return curr_task;
111:		}
112:	} 
113:
114:	p = init_task.next_run;
115:
116:#ifdef __SMP__
117:	/* skip over the ones already running */
118:	while (p->has_cpu && p != &init_task)
119:		p = p->next_run;
120:#endif
121:
122:	if (p != &init_task){
123:
124:		/* remove from front of queue and stick back on the back */
125:		next = p->next_run;
126:		prev = p->prev_run;
127:		next->prev_run = prev;
128:		prev->next_run = next;
129:		p->next_run = &init_task;
130:		prev = init_task.prev_run;
131:		init_task.prev_run = p;
132:		p->prev_run = prev;
133:		prev->next_run = p;
134:		
135:		p->counter = p->priority; 
136:		return p;
137:	}
138:#endif  /* end LINUX version */
139:	return idle_task(this_cpu);
140:}
141:
142:static int 
143:ctrr_preemptability (struct task_struct *curr_task, struct task_struct * thief, int this_cpu)
144:{
145:	return (thief->counter - curr_task->counter);
146:}
147:
148:/*-------------------------------*
149: *  load/unload module routines  *
150: *-------------------------------*/
151:
152:#ifdef MODULE
153:int  __init
154:init_module(void)
155:{
156:	int i;
157:	struct task_struct *p;
158:	struct sched_policy *new = &round_robin;
159:
160:	memset (new, 0, sizeof(struct sched_policy));
161:	new->sp_choose_task = ctrr_choose_task;
162:	new->sp_preemptability = ctrr_preemptability;
163:
164:	/* build per-cpu policy */
165:	for (i = 0; i < NR_CPUS; i++)
166:		ctrr_of_cpu[i] = new;
167:
168:	if (register_sched (CTRR_SCHED_VERSION, ctrr_of_cpu, &ctrr_fops)){
169:		printk("const_sched: unable to register\n");
170:		return -EIO;
171:	}
172:
173:	/* initialize all processes */
174:	read_lock(&tasklist_lock);
175:	for_each_task(p){
176:		p->alt_policy = new;
177:	}
178:	read_unlock(&tasklist_lock);
179:	return 0;
180:}
181:
182:void
183:cleanup_module(void)
184:{
185:	unregister_sched(CTRR_SCHED_VERSION);
186:}
187:#endif
