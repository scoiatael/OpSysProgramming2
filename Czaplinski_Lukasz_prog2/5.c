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

#define RATIO 5

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

struct thInfoPersist {
  int maxItems;
  int itemCount;
  pthread_mutex_t buffer;
  pthread_cond_t notEmpty;
  pthread_cond_t notFull;
};

struct thInfo {
  struct thFlags flags; // to distinguish between readers/writers, etc
  struct thInfoPersist* persist;
};

void setThInfo(struct thInfo* i, struct thInfoPersist* p)
{
  i->flags.data = 0;
  i->persist = p;
}

int Producer(struct thInfoPersist* t, void (*action)(void*), void* arg)
{
  CERR(pthread_mutex_lock(&t->buffer) != 0, "mutex lock failed");
  while(t->itemCount > t->maxItems - 1) { 
  //  /*
    if(cCount * RATIO < pCount) { 
      fprintf(stderr,"[+]");
      fflush(stderr);
      CERR(pthread_mutex_unlock(&t->buffer) != 0, "mutex unlock failed");
      return -1; 
    }
 //   */
    fprintf(stderr, "\n P%u : itemCount: %d maxItems: %d, C%d P%d)\n", (unsigned int)pthread_self(), t->itemCount, t->maxItems, cCount, pCount);
    CERR(pthread_cond_broadcast(&t->notEmpty), "broadcast failed");
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec+=5;
    pthread_cond_timedwait(&t->notFull, &t->buffer, &ts);
  }

  (*action)(arg);

  CERR(pthread_cond_broadcast(&t->notEmpty), "broadcast failed");
  CERR(pthread_mutex_unlock(&t->buffer) != 0, "mutex unlock failed");
  return 0;
}

int Consumer(struct thInfoPersist* t, void (*action)(void*), void* arg)
{
  CERR(pthread_mutex_lock(&t->buffer) != 0, "mutex lock failed");
  while(t->itemCount < 1) { 
      if(pCount * RATIO < cCount) {
      fprintf(stderr,"[-]");
      fflush(stderr);
      CERR(pthread_mutex_unlock(&t->buffer) != 0, "mutex unlock failed");
      return -1;
    }
    fprintf(stderr, "\n C%u : itemCount: %d maxItems: %d, C%d P%d \n", (unsigned int)pthread_self(), t->itemCount, t->maxItems, cCount, pCount);
    CERR(pthread_cond_broadcast(&t->notFull), "broadcast failed");
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec+=5;
    pthread_cond_timedwait(&t->notEmpty, &t->buffer, &ts);
  }

  (*action)(arg);

  CERR(pthread_cond_broadcast(&t->notFull), "broadcast failed");
  CERR(pthread_mutex_unlock(&t->buffer) != 0, "mutex unlock failed");
  return 0;
}

void ConsumerAction(void* info)
{
  struct thInfo* thinfo = (struct thInfo*) info;
  //Consume
  delay(100,10);
  thinfo->persist->itemCount = thinfo->persist->itemCount - 1;
  fprintf(stderr,"-");
  fflush(stderr);
  
  setAmount(&thinfo->flags, getAmount(&thinfo->flags) - 1);

  if(getAmount(&thinfo->flags) == 0) {
    fprintf(stderr,"&(%d)", cCount);
    fflush(stderr);
  }
}

void ProducerAction(void* info)
{
  struct thInfo* thinfo = (struct thInfo*) info;
  //Produce
  delay(100,10);
  thinfo->persist->itemCount = thinfo->persist->itemCount + 1;
  fprintf(stderr,"+");
  fflush(stderr);

  setAmount(&thinfo->flags, getAmount(&thinfo->flags) - 1);

  if(getAmount(&thinfo->flags) == 0) {
    fprintf(stderr,"$(%d)", pCount);
    fflush(stderr);
  }
}

void* routine(void* info)
{
  struct thInfo* thinfo = (struct thInfo*) info;
  if(isConsumer(&thinfo->flags)) {
    CriticalAdd(&cCountLock, &cCount, 1);
    int r=0;
    while(r == 0 && getAmount(&thinfo->flags) > 0 ) { r = Consumer(thinfo->persist, ConsumerAction, thinfo); }
    CriticalAdd(&cCountLock, &cCount, -1);
  }
  if(isProducer(&thinfo->flags)) {
    CriticalAdd(&pCountLock, &pCount, 1);
    int r=0;
    while(r == 0 && getAmount(&thinfo->flags) > 0 ) { r = Producer(thinfo->persist, ProducerAction, thinfo); }
    CriticalAdd(&pCountLock, &pCount, -1);
  }
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
  pthread_t threads[n];
  memset(threads, 0, sizeof(threads));
  pthread_attr_t pattr;
  CERRS(pthread_attr_init(&pattr), "pthread attr init failed");
  CERRS(pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_JOINABLE), "pthread attr setds failed");
  struct thInfo* thinfo;
  thinfo = (struct thInfo*) malloc(sizeof(struct thInfo)*n);
  memset(thinfo, 0, n*sizeof(struct thInfo));
  struct thInfoPersist* pr = (struct thInfoPersist*)malloc(sizeof(struct thInfoPersist));
  CERRS(pthread_mutex_init(&pr->buffer,NULL) != 0, "sem init failed");
  CERRS(pthread_cond_init(&pr->notEmpty,NULL) != 0, "cond init failed");
  CERRS(pthread_cond_init(&pr->notFull,NULL) != 0, "cond init failed");
  CERRS(pthread_mutex_init(&pCountLock,NULL) != 0, "mutex init failed");
  CERRS(pthread_mutex_init(&cCountLock,NULL) != 0, "mutex init failed");
  pr->maxItems = c;
  pr->itemCount = 0;
  int balance = pr->itemCount;
  cCount=0;
  pCount=0;
  for(int i=0; i < n; i++) {
    setThInfo(&thinfo[i], pr);
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
    printf("am : %u bal: %d IC: %d\n", getAmount(&thinfo[i].flags), balance, pr->itemCount);
    CERRS(pthread_create(&threads[i], &pattr, &routine, (void*) &thinfo[i]), "pthread creation failed");
  }
  for (int j=0; j < n; j++) {
//    printf("\n  joining. %d p(%d) c(%d)\n", j, pCount, cCount);
    CERRS(pthread_join(threads[j], NULL), "pthread join failed");
    printf("\nnr %d (%s) done\n", j, (isConsumer(&(thinfo[j].flags))) ? "Consumer" : "Producer");
  }
  free(thinfo);
  free(pr);
}
