#include <stdio.h>
#include <string.h>
#include "utils.c"

#define NUM_PROCESSES 20

struct process{
  /* Values initialized for each process */
  int arrivaltime;  /* Time process arrives and wishes to start */
  int runtime;      /* Time process requires to complete job */
  int priority;     /* Priority of the process */

  /* Values algorithm may use to track processes */
  int starttime;
  int endtime;
  int flag;
  int remainingtime;
};

void first_come_first_served(struct process *proc);
void shortest_remaining_time(struct process *proc);
void round_robin(struct process *proc);
void round_robin_priority(struct process *proc);
//for testing
void init_procs(struct process *proc);

int main()
{
  int i;
  struct process proc[NUM_PROCESSES],     
                 proc_copy[NUM_PROCESSES]; 

  init_procs(proc);

  printf("Process\tarrival\truntime\tpriority\n");
  for(i=0; i<NUM_PROCESSES; i++)
    printf("%d\t%d\t%d\t%d\n", i, proc[i].arrivaltime, proc[i].runtime,
           proc[i].priority);

  printf("\n\nFirst come first served\n");
  memcpy(proc_copy, proc, NUM_PROCESSES * sizeof(struct process));
  first_come_first_served(proc_copy);

  printf("\n\nShortest remaining time\n");
  memcpy(proc_copy, proc, NUM_PROCESSES * sizeof(struct process));
  shortest_remaining_time(proc_copy);
  
  printf("\n\nRound Robin\n");
  memcpy(proc_copy, proc, NUM_PROCESSES * sizeof(struct process));
  round_robin(proc_copy);

  printf("\n\nRound Robin with priority\n");
  memcpy(proc_copy, proc, NUM_PROCESSES * sizeof(struct process));
  round_robin_priority(proc_copy);

  return 0;
}

void average_time(struct process *proc){
  int i, avrg = 0;
  for(i = 0; i < NUM_PROCESSES; i++){
    avrg += proc[i].endtime - proc[i].arrivaltime;
  }
  avrg = avrg / NUM_PROCESSES;
  printf("Average time from arrival to finish is %d seconds\n", avrg);
}

void first_come_first_served(struct process *proc){
  int i, time = 0, finished = 0;
  int flag_count = 0;
  queue *q = create_queue();
  
  while(flag_count < NUM_PROCESSES){
    while(q->size != 0){
      node *n = dequeue(q);
      i = n->id;
      proc[i].starttime = time;
      printf("Process %d started at time %d\n", i, time);
      time += proc[i].runtime;
      proc[i].endtime = time;
      printf("Process %d finished at time %d\n", i, time);
      free(n);
      flag_count++;
    }
    for(i = 0; i < NUM_PROCESSES; i++){
      if(!proc[i].flag && proc[i].arrivaltime <= time){
        proc[i].flag = 1;
        node *n = create_node(i, proc[i].arrivaltime);        
        enqueue_time(q, n);
      }
    }
    if(q->size == 0){
      time++;
    }
  }
  average_time(proc);
}

void shortest_remaining_time(struct process *proc){
  int i, time = 0;
  int flag_count = 0;
  queue *q = create_queue();

  while(flag_count < NUM_PROCESSES){
    for(i = 0; i < NUM_PROCESSES; i++){
      if(proc[i].arrivaltime <= time && !proc[i].flag){
        proc[i].flag = 1;
        node *n = create_node(i, proc[i].runtime);
        enqueue_runtime(q, n);
        flag_count++;
      }
    }
    // print_queue(q);
    if(q->size == 0){
      time++;
    }
    else{
      node *n = dequeue(q);
      int id = n->id;
      printf("Process %d started at time %d\n", id, time);
      proc[id].starttime = time;
      time += proc[id].runtime;
      printf("Process %d finished at time %d\n", id, time);
      proc[id].endtime = time;
      free(n);
    }
  }
  while(q->size > 0){
    node *n = dequeue(q);
    int id = n->id;
    printf("Process %d started at time %d\n", id, time);
    proc[id].starttime = time;
    time += proc[id].runtime;
    printf("Process %d finished at time %d\n", id, time);
    proc[id].endtime = time;
    free(n);
  }
  average_time(proc);
}

void round_robin(struct process *proc){
  int i, time = 0;
  int flag_count = 0;
  int last_index = 0;
  queue *q = create_queue();
  node *n = NULL;
  
  for(i = 0; i < NUM_PROCESSES; i++){
    proc[i].remainingtime = proc[i].runtime;
  }

  while(flag_count < NUM_PROCESSES){
    // printf("starting at index %d\n", last_index);
    for(i = 0; i < NUM_PROCESSES; i++){
      int j = (last_index + i) % NUM_PROCESSES;
      if(proc[j].remainingtime > 0 && proc[j].arrivaltime <= time){
        node *temp = create_node(j, 0);
        enqueue(q, temp);
      }
    }

    if(q->size != 0){
      n = dequeue(q);
      i = n->id;
      last_index = i + 1;
      if(proc[i].runtime == proc[i].remainingtime){
        printf("Process %d started at time %d\n", i, time);
        proc[i].starttime = time;
      }
      if(proc[i].remainingtime > 10){
        proc[i].remainingtime -= 10;
        time += 10;
      }
      else{
        time += proc[i].remainingtime;
        proc[i].remainingtime = 0;
        printf("Process %d finished at time %d\n", i, time);
        proc[i].endtime = time;
        flag_count++;
        free(n);
        n = NULL;
      }
      while(q->size > 0){
        n = dequeue(q);
        free(n);
      }
    }
    else{
      time++;
    }

  }
  average_time(proc);
}

void round_robin_priority(struct process *proc){
  int i, time = 0;
  int flag_count = 0;
  int last_index = 0;
  queue *q = create_queue();
  node *n = NULL;
  
  for(i = 0; i < NUM_PROCESSES; i++){
    proc[i].remainingtime = proc[i].runtime;
  }

  while(flag_count < NUM_PROCESSES){
    // printf("starting at index %d\n", last_index);
    for(i = 0; i < NUM_PROCESSES; i++){
      int j = (last_index + i) % NUM_PROCESSES;
      if(proc[j].remainingtime > 0 && proc[j].arrivaltime <= time){
        node *temp = create_node(j, proc[j].priority);
        enqueue_priority(q, temp);
      }
    }

    if(q->size != 0){
      n = dequeue(q);
      i = n->id;
      last_index = i + 1;
      if(proc[i].runtime == proc[i].remainingtime){
        printf("Process %d started at time %d\n", i, time);
        proc[i].starttime = time;
      }
      if(proc[i].remainingtime > 10){
        proc[i].remainingtime -= 10;
        time += 10;
      }
      else{
        time += proc[i].remainingtime;
        proc[i].remainingtime = 0;
        printf("Process %d finished at time %d\n", i, time);
        proc[i].endtime = time;
        flag_count++;
        free(n);
        n = NULL;
      }
      while(q->size > 0){
        n = dequeue(q);
        free(n);
      }
    }
    else{
      time++;
    }

  }
  average_time(proc);
}

void init_procs(struct process *proc){
  int i;
  for(i = 0; i < 20; i++){
    proc[i].priority = 0;
    proc[i].starttime = 0;
    proc[i].endtime = 0;
    proc[i].flag = 0;
    proc[i].remainingtime = 0;
  }
  
  // 0 	10	25	0
  proc[0].arrivaltime = 10;
  proc[0].runtime = 25;
  
  // 1 	69 	36 	2
  proc[1].arrivaltime = 69;
  proc[1].runtime = 36;
  proc[1].priority = 2;

  // 2 	87 	20 	0
  proc[2].arrivaltime = 87;
  proc[2].runtime = 20;

  // 3 	1 	16 	2
  proc[3].arrivaltime = 1;
  proc[3].runtime = 16;
  proc[3].priority = 2;

  // 4 	46 	28 	0
  proc[4].arrivaltime = 46;
  proc[4].runtime = 28;

  // 5 	92 	14 	1
  proc[5].arrivaltime = 92;
  proc[5].runtime = 14;
  proc[5].priority = 1;

  // 6 	74 	12 	1
  proc[6].arrivaltime = 74;
  proc[6].runtime = 12;
  proc[6].priority = 1;

  // 7 	61 	28 	0
  proc[7].arrivaltime = 61;
  proc[7].runtime = 28;

  // 8 	89 	27 	0
  proc[8].arrivaltime = 89;
  proc[8].runtime = 27;

  // 9 	28 	31 	1
  proc[9].arrivaltime = 28;
  proc[9].runtime = 31;
  proc[9].priority = 1;

  // 10 	34 	33 	2
  proc[10].arrivaltime = 34;
  proc[10].runtime = 33;
  proc[10].priority = 2;

  // 11 	82 	13 	1
  proc[11].arrivaltime = 82;
  proc[11].runtime = 13;
  proc[11].priority = 1;

  // 12 	93 	32 	0
  proc[12].arrivaltime = 93;
  proc[12].runtime = 32;

  // 13 	85 	33 	0
  proc[13].arrivaltime = 85;
  proc[13].runtime = 33;

  // 14 	87 	11 	1
  proc[14].arrivaltime = 87;
  proc[14].runtime = 11;
  proc[14].priority = 1;

  // 15 	57 	35 	1
  proc[15].arrivaltime = 57;
  proc[15].runtime = 35;
  proc[15].priority = 1;

  // 16 	2 	10 	0
  proc[16].arrivaltime = 2;
  proc[16].runtime = 10;

  // 17 	27 	31 	0
  proc[17].arrivaltime =27;
  proc[17].runtime = 31;

  // 18 	34 	10 	0
  proc[18].arrivaltime = 34;
  proc[18].runtime = 10;

  // 19 	78 	18 	1
  proc[19].arrivaltime = 78;
  proc[19].runtime = 18;
  proc[19].priority = 1;

}