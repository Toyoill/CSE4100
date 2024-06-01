/*
 * echoserveri.c - An iterative echo server
 */
/* $begin echoserverimain */
#include "process.h"

void Init(int listen_fd, struct pool *p);
void sigint_handler(int sig);

int main(int argc, char **argv) {
  int listenfd, connfd;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  /* Enough space for any address */  // line:netp:echoserveri:sockaddrstorage
  char client_hostname[MAXLINE], client_port[MAXLINE];

  struct pool pool;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(0);
  }

  listenfd = Open_listenfd(argv[1]);

  Init(listenfd, &pool);

  while (1) {
    pool.ready_set = pool.read_set;
    pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);

    if (FD_ISSET(listenfd, &pool.ready_set)) {
      clientlen = sizeof(struct sockaddr_storage);
      connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
      AddClient(connfd, &pool);
      Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE,
                  client_port, MAXLINE, 0);
      printf("Connected to (%s, %s)\n", client_hostname, client_port);
    }

    CheckClients(&pool);
  }
  exit(0);
}

void Init(int listen_fd, struct pool *p) {
  InitPool(listen_fd, p);
  Signal(SIGINT, sigint_handler);
  InitStock();
}

void sigint_handler(int sig) {
  CleanStock();
  exit(0);
}

/* $end echoserverimain */
