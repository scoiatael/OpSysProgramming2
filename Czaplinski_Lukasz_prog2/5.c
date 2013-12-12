#include "common.h"
#include <sys/stat.h>

int pCount,cCount;
pthread_mutex_t pCountLock, cCountLock;

void CriticalAdd(pthread_mutex_t* m, int* crit, int amount)
{
  CERR(pthread_mutex_lock(m) != 0, "mutex lock failed");
  (*crit) = (*crit) + amount;
  CERR(pthread_mutex_unlock(m) != 0, "mutex unlock failed");
}

struct thFlags {
  char data;
};

#define ConsumerMask 128
char isConsumer(struct thFlags* f)
{
  return f->data & ConsumerMask;
}

void setConsumer(struct thFlags* f, unsigned char b)
{
  if(b > 0) {
    f->data = f->data | ConsumerMask; 
  } else {
    f->data = f->data & (~ConsumerMask);
  }
}

#define ProducerMask 64
char isProducer(struct thFlags* f)
{
  return f->data & ProducerMask;
}

void setProducer(struct thFlags* f, unsigned char b)
{
  if(b > 0) {
    f->data = f->data | ProducerMask; 
  } else {
    f->data = f->data & (~ProducerMask);
  }
}

#define AmountMask 63
unsigned char getAmount(struct thFlags* f)
{
  return f->data & AmountMask;
}

void setAmount(struct thFlags* f, unsigned char b)
{
  f->data = (f->data & (~AmountMask)) | (b & AmountMask);
}

struct thInfo {
  struct thFlags flags; // to distinguish between readers/writers, etc
  int *maxItems;
  int *itemCount;
  pthread_mutex_t *buffer;
  pthread_cond_t *notEmpty;
  pthread_cond_t *notFull;
  unsigned long long time;
};

void setThInfo(struct thInfo* i, int* mi, int* iC, pthread_mutex_t* b, pthread_cond_t* nE, pthread_cond_t* nF, int t)
{
  i->flags.data = 0;
  i->maxItems = mi;
  i->itemCount = iC;
  i->buffer = b;
  i->notEmpty = nE;
  i->notFull = nF;
  i->time = t;
}

void Producer(struct thInfo* t)
{
  CriticalAdd(&pCountLock, &pCount, 1);
  int am = getAmount(&t->flags);
  setAmount(&t->flags, 1);
  for(; am > 0; am = am - 1) {
    CERR(pthread_mutex_lock(t->buffer) != 0, "mutex lock failed");
    while(*t->itemCount > *t->maxItems - getAmount(&t->flags)) { 
    //  /*
      if(cCount == 0) { 
        CERR(pthread_mutex_unlock(t->buffer) != 0, "mutex unlock failed");
        return; 
      }
   //   */
      fprintf(stderr, "\n P%u : itemCount: %d maxItems: %d, left %d, %d consumers)\n", (unsigned int)pthread_self(), *t->itemCount, *t->maxItems, am, cCount);
      CERR(pthread_cond_broadcast(t->notEmpty), "broadcast failed");
      CERR(pthread_cond_wait(t->notFull, t->buffer), "cond wait failed");
    }

    //Produce
    delay(100,10);
    *t->itemCount = *t->itemCount + getAmount(&t->flags);
    fprintf(stderr,"+");
    fflush(stderr);

    if(am == 1) {
      CriticalAdd(&pCountLock, &pCount, -1);
      fprintf(stderr,"$(%d)", pCount);
      fflush(stderr);
    }

    CERR(pthread_cond_broadcast(t->notEmpty), "broadcast failed");
    CERR(pthread_mutex_unlock(t->buffer) != 0, "mutex unlock failed");
  }
}

void Consumer(struct thInfo* t)
{
  CriticalAdd(&cCountLock, &cCount, 1);
  int am = getAmount(&t->flags);
  setAmount(&t->flags, 1);
  for(; am > 0; am = am - 1) {
    CERR(pthread_mutex_lock(t->buffer) != 0, "mutex lock failed");
    while(*t->itemCount < getAmount(&t->flags)) { 
        if(pCount == 0) {
        CERR(pthread_mutex_unlock(t->buffer) != 0, "mutex unlock failed");
        return;
      }
      fprintf(stderr, "\n C%u : itemCount: %d maxItems: %d, left %d, %d producers \n", (unsigned int)pthread_self(), *t->itemCount, *t->maxItems, am, pCount);
      CERR(pthread_cond_broadcast(t->notFull), "broadcast failed");
      CERR(pthread_cond_wait(t->notEmpty, t->buffer), "cond wait failed");
    }

    //Consume
    delay(100,10);
    *t->itemCount = *t->itemCount - getAmount(&t->flags);
    fprintf(stderr,"-");
    fflush(stderr);

    if(am == 1) {
      CriticalAdd(&cCountLock, &cCount, -1);
      fprintf(stderr,"&(%d)", cCount);
      fflush(stderr);
    }

    CERR(pthread_cond_broadcast(t->notFull), "broadcast failed");
    CERR(pthread_mutex_unlock(t->buffer) != 0, "mutex unlock failed");
  }
}

void* routine(void* info)
{
  struct thInfo* thinfo = (struct thInfo*) info;
  unsigned long long i;
  DIFF(
  if(isConsumer(&thinfo->flags)) {
    Consumer(thinfo);
  }
  if(isProducer(&thinfo->flags)) {
    Producer(thinfo);
  }, i);
  thinfo->time = i;
  return NULL;
}

#define GRAIN 10 
int main(int argc, char* const argv[])
{
  srand(time(NULL));
  int n=10, m=255 & AmountMask, p=GRAIN, c = n*m/4; 
  int gopt;
  while((gopt = getopt(argc,argv,"n:m:p:c:")) != -1) {
    switch(gopt) {
      case 'n':
        n = atoi(optarg);
        break;
      case 'm':
        m = atoi(optarg);
        break;
      case 'p':
        p = atoi(optarg);
        break;
      case 'c':
        c = atoi(optarg);
        break;
    }
  }
  if(m > (255 & AmountMask)) {
    printf("Max m is %d\n", (255 & AmountMask));
  }
  printf("n: %d (thread total nr)  m: %d (max production/consumption) p: %d (chance) c: %d (maxItems)\n", n,m,p,c);
  pthread_t threads[n*2];
  memset(threads, 0, sizeof(threads));
  pthread_attr_t pattr;
  CERRS(pthread_attr_init(&pattr), "pthread attr init failed");
  CERRS(pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_JOINABLE), "pthread attr setds failed");
  struct thInfo thinfo[n*2];
  memset(&thinfo, 0, sizeof(struct thInfo));
  pthread_mutex_t buffer;
  pthread_cond_t notEmpty, notFull;
  CERRS(pthread_mutex_init(&buffer,NULL) != 0, "sem init failed");
  CERRS(pthread_cond_init(&notEmpty,NULL) != 0, "cond init failed");
  CERRS(pthread_cond_init(&notFull,NULL) != 0, "cond init failed");
  CERRS(pthread_mutex_init(&pCountLock,NULL) != 0, "mutex init failed");
  CERRS(pthread_mutex_init(&cCountLock,NULL) != 0, "mutex init failed");
  int maxItems = c;
  int itemCount = 0;
  int balance = itemCount;
  cCount=0;
  pCount=0;
  for(int i=0; i < n; i++) {
    setThInfo(&thinfo[i], &maxItems, &itemCount, &buffer, &notEmpty, &notFull, 0);
    setAmount(&thinfo[i].flags, rand() % m);
    if(rand() % (p+GRAIN) > GRAIN ) {
      printf("Made consumer: ");
      setConsumer(&thinfo[i].flags, 1);
      balance = balance - getAmount(&thinfo[i].flags);
    } else {
      printf("Made producer: ");
      setProducer(&thinfo[i].flags, 1);
      balance = balance + getAmount(&thinfo[i].flags);
    }
    printf("am : %u bal: %d IC: %d\n", getAmount(&thinfo[i].flags), balance, itemCount);
    CERRS(pthread_create(&threads[i], &pattr, &routine, (void*) &thinfo[i]), "pthread creation failed");
  }
  setThInfo(&thinfo[n], &maxItems, &itemCount, &buffer, &notEmpty, &notFull, 0);
  setAmount(&thinfo[n].flags, m);
  if(balance >= m) {
    printf("Rebalancing : C..\n");
    setConsumer(&thinfo[n].flags, 1);
  }
  if(balance < -m) {
    printf("Rebalancing : P..\n");
    setProducer(&thinfo[n].flags, 1);
  }
  int aux_assert = (balance > 0) ? balance : -balance, addI;
  for(addI=0; aux_assert > m; aux_assert = aux_assert - m, addI++) {
    printf(" rebalance nr %d : %d.. %s\n", addI, aux_assert, (isConsumer(&thinfo[n].flags)) ? "C" : "P");
    CERRS(pthread_create(&threads[n+addI], &pattr, &routine, (void*) &thinfo[n]), "pthread creation failed");
  }
  if(aux_assert != 0) {
    //addI++;
    printf("Balance bf last balance: %d\n", aux_assert);
    assert(aux_assert > 0);
    setThInfo(&thinfo[n+1], &maxItems, &itemCount, &buffer, &notEmpty, &notFull, 0);
    setAmount(&thinfo[n+1].flags, aux_assert);
    if(balance > 0) {
      setConsumer(&thinfo[n+1].flags, 1);
    } else {
      setProducer(&thinfo[n+1].flags, 1);
    }
    CERRS(pthread_create(&threads[n + addI], &pattr, &routine, (void*) &thinfo[n+1]), "pthread creation failed");
  }
  for (int j=0; j < n + addI; j++) {
    printf("\n  joining. %d p(%d) c(%d)\n", j, pCount, cCount);
    CERRS(pthread_join(threads[j], NULL), "pthread join failed");
    printf("\nnr %d (%s) took %llu time.\n", j, (isConsumer(&thinfo[j].flags)) ? "Consumer" : "Producer", thinfo[j].time);
  }
}
