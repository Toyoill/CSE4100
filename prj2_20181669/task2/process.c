/*
 * echo - read and echo text lines until client closes connection
 */
/* $begin echo */

#include "process.h"

int Process(int connfd, rio_t rio) {
  int n, ID, amount;
  char buf[MAXLINE], *ptr;

  if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
    printf("server received %d bytes\n", n);

    if (buf[n - 1] == '\n') buf[--n] = '\0';

    ptr = strtok(buf, " ");

    if (strcmp(ptr, "show") == 0) {
      buf[0] = '\0';
      Show(NULL, buf, connfd);
    } else if (strcmp(ptr, "buy") == 0) {
      ID = atoi(strtok(NULL, " "));
      amount = atoi(strtok(NULL, " "));
      Buy(ID, amount, buf);

    } else if (strcmp(ptr, "sell") == 0) {
      ID = atoi(strtok(NULL, " "));
      amount = atoi(strtok(NULL, " "));
      Sell(ID, amount, buf);
    } else {
      sprintf(buf, "Invalid request\n");
    }
    Rio_writen(connfd, buf, MAXLINE);

    return 0;
  }
  return -1;
}

void sbuf_init(struct sbuf_t *sp, int n) {
  sp->buf = Calloc(n, sizeof(int));
  sp->n = n;
  sp->front = sp->rear = 0;
  Sem_init(&sp->mutex, 0, 1);
  Sem_init(&sp->slots, 0, n);
  Sem_init(&sp->items, 0, 0);
}

void sbuf_deinit(struct sbuf_t *sp) { Free(sp->buf); }
void sbuf_insert(struct sbuf_t *sp, int connfd) {
  P(&sp->slots);
  P(&sp->mutex);
  sp->buf[(++sp->rear) % (sp->n)] = connfd;
  V(&sp->mutex);
  V(&sp->items);
}

int sbuf_remove(struct sbuf_t *sp) {
  int connfd;
  P(&sp->items);
  P(&sp->mutex);
  connfd = sp->buf[(++sp->front) % (sp->n)];
  V(&sp->mutex);
  V(&sp->slots);
  return connfd;
}

void *thread(void *vargp) {
  struct sbuf_t *sbufp = vargp;
  rio_t rio;

  Pthread_detach(pthread_self());

  while (1) {
    int connfd = sbuf_remove(sbufp);
    Rio_readinitb(&rio, connfd);
    while (Process(connfd, rio) != -1)
      ;
    Close(connfd);
  }
}