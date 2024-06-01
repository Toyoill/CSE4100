#include "stock.h"

void InitStock() {
  char buf[MAXBUF];
  int bufLen, ID, remain, price;
  FILE* fp;

  stocks = NULL;

  if ((fp = fopen("stock.txt", "r")) == NULL) return;

  while (fgets(buf, MAXBUF, fp)) {
    bufLen = strlen(buf);
    if (buf[bufLen - 1] == '\n') buf[--bufLen] = '\0';

    ID = atoi(strtok(buf, " "));
    remain = atoi(strtok(NULL, " "));
    price = atoi(strtok(NULL, " "));

    Insert(ID, remain, price);
  }

  fclose(fp);
}

void Insert(int ID, int remain, int price) {
  struct node *newNode, **pos;

  newNode = (struct node*)malloc(sizeof(struct node));
  newNode->leftChild = NULL;
  newNode->rightChild = NULL;
  newNode->stock.ID = ID;
  newNode->stock.left_stock = remain;
  newNode->stock.price = price;
  newNode->stock.readcnt = 0;
  Sem_init(&newNode->stock.mutex, 0, 1);
  Sem_init(&newNode->stock.w, 0, 1);

  if (*(pos = Search(ID)) != NULL) {
    fprintf(stderr, "There is a duplicate stock ID: %d\n", ID);
    return;
  }

  *pos = newNode;
}

struct node** Search(int ID) {
  struct node *tmp = stocks, **ret = &stocks;
  while (tmp) {
    if (tmp->stock.ID < ID) {
      ret = &(tmp->rightChild);
      tmp = tmp->rightChild;
    } else if (ID < tmp->stock.ID) {
      ret = &(tmp->leftChild);
      tmp = tmp->leftChild;
    } else
      break;
  }
  return ret;
}

void Show(struct node* cur, char* buf, int connfd) {
  char tmp[MAXLINE];

  if (!cur) cur = stocks;

  P(&cur->stock.mutex);
  cur->stock.readcnt++;
  if (cur->stock.readcnt == 1) P(&cur->stock.w);
  V(&cur->stock.mutex);

  sprintf(tmp, "%d %d %d\n", cur->stock.ID, cur->stock.left_stock,
          cur->stock.price);

  P(&cur->stock.mutex);
  cur->stock.readcnt--;
  if (cur->stock.readcnt == 0) V(&cur->stock.w);
  V(&cur->stock.mutex);

  strcat(buf, tmp);

  if (cur->leftChild) Show(cur->leftChild, buf, connfd);
  if (cur->rightChild) Show(cur->rightChild, buf, connfd);
}

void Buy(int ID, int amount, char* buf) {
  struct node** target;
  if (*(target = Search(ID)) == NULL) {
    sprintf(buf, "Cannot find target\n");
    return;
  }

  P(&((*target)->stock.w));
  if (amount < 0) {
    sprintf(buf, "invalid amount\n");
  } else if ((*target)->stock.left_stock < amount) {
    sprintf(buf, "Not enough left stock\n");
  } else {
    (*target)->stock.left_stock -= amount;
    sprintf(buf, "[buy] success\n");
  }

  V(&((*target)->stock.w));
}

void Sell(int ID, int amount, char* buf) {
  struct node** target;
  if (*(target = Search(ID)) == NULL) {
    sprintf(buf, "Cannot find target\n");
    return;
  }

  P(&((*target)->stock.w));

  (*target)->stock.left_stock += amount;
  sprintf(buf, "[sell] success\n");

  V(&((*target)->stock.w));
}

void CleanStock() {
  int idx = 0;
  struct item items[MAX_STOCK + 1];
  FILE* fp;

  Decompose(items, &idx, stocks);

  if ((fp = fopen("stock.txt", "w")) == NULL) {
    fprintf(stderr, "Failed to open file\n");
    return;
  }

  for (int i = 0; i < idx; i++) {
    fprintf(fp, "%d %d %d", items[i].ID, items[i].left_stock, items[i].price);
    if (i != idx - 1) fprintf(fp, "\n");
  }

  fclose(fp);
}

void Decompose(struct item* items, int* idx, struct node* cur) {
  items[(*idx)++] = cur->stock;

  if (cur->leftChild) Decompose(items, idx, cur->leftChild);
  if (cur->rightChild) Decompose(items, idx, cur->rightChild);

  free(cur);
}