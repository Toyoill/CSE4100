/*
 * echoserveri.c - An iterative echo server
 */
/* $begin echoserverimain */
#include "process.h"

struct sbuf_t sbuf;

void Init(int listen_fd, int n);
void sigint_handler(int sig);

int main(int argc, char **argv) {
  int listenfd, connfd;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  /* Enough space for any address */  // line:netp:echoserveri:sockaddrstorage
  char client_hostname[MAXLINE], client_port[MAXLINE];

  pthread_t tid;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(0);
  }

  listenfd = Open_listenfd(argv[1]);

  Init(listenfd, SBUFSIZE);
  for (int i = 0; i < NTHREADS; i++) Pthread_create(&tid, NULL, thread, &sbuf);

  while (1) {
    clientlen = sizeof(struct sockaddr_storage);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    sbuf_insert(&sbuf, connfd);
    Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE,
                client_port, MAXLINE, 0);
    printf("Connected to (%s, %s)\n", client_hostname, client_port);
  }
  exit(0);
}

void Init(int listen_fd, int n) {
  sbuf_init(&sbuf, n);
  Signal(SIGINT, sigint_handler);
  InitStock();
}

void sigint_handler(int sig) {
  CleanStock();
  sbuf_deinit(&sbuf);
  exit(0);
}

/* $end echoserverimain */
