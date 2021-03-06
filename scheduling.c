/*******************************************************************************
*
* CprE 308 Scheduling Lab
*
* scheduling.c
*******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "utils.c"

#define NUM_PROCESSES 20

struct process
{
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

/* Forward declarations of Scheduling algorithms */
void first_come_first_served(struct process *proc);
void shortest_remaining_time(struct process *proc);
void round_robin(struct process *proc);
void round_robin_priority(struct process *proc);

int main()
{
  int i;
  struct process proc[NUM_PROCESSES],      /* List of processes */
                 proc_copy[NUM_PROCESSES]; /* Backup copy of processes */

  /* Seed random number generator */
  /*srand(time(0));*/  /* Use this seed to test different scenarios */
  srand(0xC0FFEE);     /* Used for test to be printed out */

  /* Initialize process structures */
  for(i=0; i<NUM_PROCESSES; i++)
  {
    proc[i].arrivaltime = rand()%100;
    proc[i].runtime = (rand()%30)+10;
    proc[i].priority = rand()%3;
    proc[i].starttime = 0;
    proc[i].endtime = 0;
    proc[i].flag = 0;
    proc[i].remainingtime = 0;
  }

  /* Show process values */
  printf("Process\tarrival\truntime\tpriority\n");
  for(i=0; i<NUM_PROCESSES; i++)
    printf("%d\t%d\t%d\t%d\n", i, proc[i].arrivaltime, proc[i].runtime,
           proc[i].priority);

  /* Run scheduling algorithms */
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