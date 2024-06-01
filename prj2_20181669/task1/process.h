#ifndef __PROCESS_H__
#define __PROCESS_H__

#include "csapp.h"
#include "stock.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

struct client_t {
  int fd;
  rio_t clientrio;
  struct client_t* next;
};

struct pool {
  int maxfd;
  int nready;
  fd_set read_set;
  fd_set ready_set;
  struct client_t* clientPtr;
};

int Process(int connfd, rio_t rio);
void InitPool(int listen_fd, struct pool* p);
void AddClient(int connfd, struct pool* p);
void CheckClients(struct pool* p);

#endif