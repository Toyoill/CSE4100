#ifndef __STOCK_H__
#define __STOCK_H__

#include "csapp.h"

#define MAX_STOCK 100

struct item {
  int ID;
  int left_stock;
  int price;
  int readcnt;
  sem_t mutex, w;
};

struct node {
  struct node* leftChild;
  struct node* rightChild;
  struct item stock;
};

int size;
struct node* stocks;

void InitStock();
void Insert(int ID, int remain, int price);
/*
struct node** Search(int id);

id에 해당하는 node를 찾는다.
존재한다면 해당하는 node의 주소의 pointer를 반환한다.
존재하지 않는다면 해당하는 id를 저장할 수 있는 주소의 pointer를 반환한다.
*/
struct node** Search(int ID);
/*
void Show(struct node* cur, char* buf);

Usage: Show(NULL, buf);
*/
void Show(struct node* cur, char* buf, int connfd);
void Buy(int ID, int amount, char* buf);
void Sell(int ID, int amount, char* buf);
void CleanStock();
void Decompose(struct item* items, int* idx, struct node* cur);

#endif
