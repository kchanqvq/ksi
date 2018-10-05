#ifndef QUEUE_H
#define QUEUE_H

#include "lcrq.h"

void queue_init(queue_t * q, int nprocs);
void queue_register(queue_t * q, handle_t * th, int id);
void enqueue(queue_t * q, handle_t * th, void * v);
void * dequeue(queue_t * q, handle_t * th);
void queue_free(queue_t * q, handle_t * h);
void handle_free(handle_t *h);


#endif /* end of include guard: QUEUE_H */
