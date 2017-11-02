/*******************************************************************************
*
* CprE 308 Scheduling Lab
*
* scheduling.c
*******************************************************************************/
#include <stdio.h>
#include <string.h>

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
  /**
  printf("\n\nFirst come first served\n");
  memcpy(proc_copy, proc, NUM_PROCESSES * sizeof(struct process));
  first_come_first_served(proc_copy);
  **/

  printf("\n\nShortest remaining time\n");
  memcpy(proc_copy, proc, NUM_PROCESSES * sizeof(struct process));
  shortest_remaining_time(proc_copy);
  
  /**
  printf("\n\nRound Robin\n");
  memcpy(proc_copy, proc, NUM_PROCESSES * sizeof(struct process));
  round_robin(proc_copy);

  printf("\n\nRound Robin with priority\n");
  memcpy(proc_copy, proc, NUM_PROCESSES * sizeof(struct process));
  round_robin_priority(proc_copy);
  **/
  return 0;
}

void set_remainingtime(struct process *proc){
  int i;
  for(i = 0; i < NUM_PROCESSES; i++){
    proc[i].remainingtime = proc[i].runtime;
  }
}

void reset(struct process *proc){
  int i;
  for(i = 0; i < NUM_PROCESSES; i++){
    proc[i].starttime = 0;
    proc[i].endtime = 0;
    proc[i].flag = 0;
    proc[i].remainingtime = 0;
  }
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
  int next = 0;
  
  while(flag_count < NUM_PROCESSES){
    for(i = 0; i < NUM_PROCESSES; i++){
      int arr = proc[i].arrivaltime;
      if(!proc[i].flag && arr == next && time >= arr){
        proc[i].starttime = time;
        printf("Process %d started at time %d\n", i, time);
        time += proc[i].runtime;
        proc[i].endtime = time;
        proc[i].flag = 1;
        printf("Process %d finished at time %d\n", i, time);
        next--;
        i = NUM_PROCESSES;
        flag_count++;
      }
    }
    if(time == next){
      time++;
    }
    next++;
  }
  average_time(proc);
  reset(proc);
}

void shortest_remaining_time(struct process *proc){
  set_remainingtime(proc);
  int i, time = 0, finished = 0;
  int flag_count = 0;
  int next = 0;
  
  while(flag_count < NUM_PROCESSES){
    for(i = 0; i < NUM_PROCESSES; i++){
      int arr = proc[i].arrivaltime;
      if(!proc[i].flag && arr == next && time >= arr){
        proc[i].starttime = time;
        printf("Process %d started at time %d\n", i, time);
        time += proc[i].runtime;
        proc[i].endtime = time;
        proc[i].flag = 1;
        printf("Process %d finished at time %d\n", i, time);
        next--;
        i = NUM_PROCESSES;
        flag_count++;
      }
    }
    if(time == next){
      time++;
    }
    next++;
  }
  average_time(proc);
  reset(proc);
}

void round_robin(struct process *proc)
{
  /* Implement scheduling algorithm here */
}

void round_robin_priority(struct process *proc)
{
  /* Implement scheduling algorithm here */
}

