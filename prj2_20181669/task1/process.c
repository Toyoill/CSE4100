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

void InitPool(int listen_fd, struct pool* p) {
  p->maxfd = listen_fd;
  p->clientPtr = NULL;
  FD_ZERO(&p->read_set);
  FD_SET(listen_fd, &p->read_set);
}

void AddClient(int connfd, struct pool* p) {
  struct client_t* newClient =
      (struct client_t*)malloc(sizeof(struct client_t));

  newClient->fd = connfd;
  Rio_readinitb(&newClient->clientrio, newClient->fd);
  newClient->next = p->clientPtr;
  p->clientPtr = newClient;

  FD_SET(connfd, &p->read_set);

  p->maxfd = MAX(connfd, p->maxfd);
}

void CheckClients(struct pool* p) {
  int connfd, removed;
  rio_t rio;
  struct client_t *tmp = p->clientPtr, *prev = NULL, *dump;

  while (tmp && p->nready > 0) {
    removed = 0;

    connfd = tmp->fd;
    rio = tmp->clientrio;

    if (FD_ISSET(connfd, &p->ready_set)) {
      p->nready--;
      if (Process(connfd, rio) < 0) {
        removed = 1;

        Close(connfd);
        FD_CLR(connfd, &p->read_set);

        if (prev)
          prev->next = tmp->next;
        else
          p->clientPtr = tmp->next;
        dump = tmp;
        tmp = tmp->next;
        free(dump);
      }
    }

    if (!removed) {
      prev = tmp;
      tmp = tmp->next;
    }
  }
}
/* $end echo */
