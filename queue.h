#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>

#define QUEUE_SIZE 32  // Adjust size as needed

typedef struct {
    char data[QUEUE_SIZE];
    int front;
    int rear;
    int count;
} Queue;

// Initialize the queue
void queue_init(Queue* q);

// Push an item to the queue
// Returns true if successful, false if queue is full
bool queue_push(Queue* q, char item);

// Pop an item from the queue
// Returns true if successful, false if queue is empty
// The popped item is stored in the location pointed to by item
bool queue_pop(Queue* q, char* item);

// Check if queue is empty
bool queue_is_empty(Queue* q);

// Check if queue is full
bool queue_is_full(Queue* q);

void queue_init(Queue* q) {
    q->front = 0;
    q->rear = -1;
    q->count = 0;
}

bool queue_push(Queue* q, char item) {
    if (queue_is_full(q)) {
        return false;
    }
    
    q->rear = (q->rear + 1) % QUEUE_SIZE;
    q->data[q->rear] = item;
    q->count++;
    return true;
}

bool queue_pop(Queue* q, char* item) {
    if (queue_is_empty(q)) {
        return false;
    }
    
    *item = q->data[q->front];
    q->front = (q->front + 1) % QUEUE_SIZE;
    q->count--;
    return true;
}

bool queue_is_empty(Queue* q) {
    return (q->count == 0);
}

bool queue_is_full(Queue* q) {
    return (q->count == QUEUE_SIZE);
}

#endif // QUEUE_H
