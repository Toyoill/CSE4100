#ifndef __PROCESS_H__
#define __PROCESS_H__

#include "csapp.h"
#include "stock.h"

#define SBUFSIZE 100
#define NTHREADS 20
#define MAX(a, b) ((a) > (b) ? (a) : (b))

struct sbuf_t {
  int *buf;
  int n;
  int front;
  int rear;
  sem_t mutex;
  sem_t slots;
  sem_t items;
};

int Process(int connfd, rio_t rio);
void sbuf_init(struct sbuf_t *sp, int n);
void sbuf_deinit(struct sbuf_t *sp);
void sbuf_insert(struct sbuf_t *sp, int connfd);
int sbuf_remove(struct sbuf_t *sp);
void *thread(void *vargp);

#endif