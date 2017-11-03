#include <stdio.h>

typedef struct node{
  int id;
  int value;
  struct node *next;
}node;

typedef struct queue{
  node *head;
  int size;
}queue;

node *create_node(int i, int v){
  node *n = (node*)malloc(sizeof(node));
  n->id = i;
  n->value = v;
  n->next = NULL;
  return n;
}

queue *create_queue(){
  queue *temp = (queue*)malloc(sizeof(queue));
  temp->size = 0;
  temp->head = NULL;
  return temp;
}

node *dequeue(queue *q){
  node *temp = q->head;
  q->head = temp->next;
  q->size--;
  temp->next = NULL;
  return temp;
}

void enqueue(queue *q, node *n){
  if(q->size == 0){
    q->head = n;
  }
  else{
    node *a = q->head;
    while(a->next != NULL){
      a = a->next;
    }
    a->next = n;
  }
  q->size++;
}

void enqueue_time(queue *q, node *n){
  if(q->size == 0){
    q->head = n;
  }
  else{
    node *a = NULL;
    node *b = q->head;
    int finished = 0;
    while(b != NULL && !finished){
      if(b->value > n->value){
        if(a != NULL){
          a->next = n;
        }else{
          q->head = n;
        }
        n->next = b;
        finished = 1;
      }
      else if(b->next == NULL){
        b->next = n;
        finished = 1;
      }
      a = b;
      b = b->next;
    }
  }
  q->size++;
}

void enqueue_runtime(queue *q, node *n){
  // printf("%d:\t", n->id);
  if(q->size == 0){
    q->head = n;
    // printf("Starting queue\n");
  }
  else{
    node *a = NULL;
    node *b = q->head;
    int finished = 0;
    while(b != NULL && !finished){
      if(b->value > n->value){
        //printf("Inserting into queue\n");
        if(a != NULL){
          a->next = n;
        }else{
          q->head = n;
        }
        n->next = b;
        finished = 1;
      }
      else if(b->next == NULL){
        // printf("Putting to end of queue\n");
        b->next = n;
        finished = 1;
      }
      a = b;
      b = b->next;
    }
  }
  q->size++;
}

void enqueue_priority(queue *q, node *n){
  // printf("%d:\t", n->id);
  if(q->size == 0){
    q->head = n;
    // printf("Starting queue\n");
  }
  else{
    node *a = NULL;
    node *b = q->head;
    int finished = 0;
    while(b != NULL && !finished){
      if(b->value < n->value){
        //printf("Inserting into queue\n");
        if(a != NULL){
          a->next = n;
        }else{
          q->head = n;
        }
        n->next = b;
        finished = 1;
      }
      else if(b->next == NULL){
        // printf("Putting to end of queue\n");
        b->next = n;
        finished = 1;
      }
      a = b;
      b = b->next;
    }
  }
  q->size++;
}

void print_queue(queue *queue){
  node *temp = queue->head;
  if(queue->size > 0){
    int id, rt, p;
    while(temp != NULL){
      id = temp->id;
      rt = temp->value;
      printf("[ id : %d, value : %d ]\n", id, rt);
      temp = temp->next;
    }
    printf("------------------------\n");
    printf("------------------------\n");
  }
  else{
    printf("Empty\n");
  }
  
}