#include "common.h"

struct thFlags {
  char data;
};

#define ReaderMask 1
char isReader(struct thFlags* f)
{
  return f->data & ReaderMask;
}

void setReader(struct thFlags* f, unsigned char b)
{
  if(b > 0) {
    f->data = f->data | ReaderMask; 
  } else {
    f->data = f->data & (~ReaderMask);
  }
}

#define WriterMask 2
char isWriter(struct thFlags* f)
{
  return f->data & WriterMask;
}

void setWriter(struct thFlags* f, unsigned char b)
{
  if(b > 0) {
    f->data = f->data | WriterMask; 
  } else {
    f->data = f->data & (~WriterMask);
  }
}

struct thInfo {
  struct thFlags *flags; // to distinguish between readers/writers, etc
  pthread_mutex_t wSem; // to control reading/writing
  int readerNr; // to count readers
  pthread_mutex_t rSem; // to control access to reader count
  int readerMaxWait;
  pthread_mutex_t rmSem;
  int writerMaxWait;
  pthread_mutex_t wmSem;
};

void writeIfHigher(int val, int* where, pthread_mutex_t* sem)
{
  CERRS(pthread_mutex_lock(sem) != 0, "wIH sem lock failed");
  if(*where < val) {
    *where = val;
  } 
  CERRS(pthread_mutex_unlock(sem) != 0, "wiH sem unlock failed");
}

void Writer(struct thInfo* t)
{
  CERRS(pthread_mutex_lock(&t->wSem) != 0, "wSem lock failed");

  delay(100,10);

  CERRS(pthread_mutex_unlock(&t->wSem) != 0, "wSem unlock failed");
}

void Reader(struct thInfo* t)
{
  CERRS(pthread_mutex_lock(&t->rSem) != 0, "rSem lock failed");
  (t->readerNr)++;
  if(t->readerNr == 1) {
    CERRS(pthread_mutex_lock(&t->wSem) != 0, "wSem lock failed");
  }
  CERRS(pthread_mutex_unlock(&t->rSem) != 0, "rSem unlock failed");
  
  delay(1000,10);

  CERRS(pthread_mutex_lock(&t->rSem) != 0, "rSem lock failed");
  t->readerNr = t->readerNr - 1;
  if(t->readerNr == 0) {
    CERRS(pthread_mutex_unlock(&t->wSem) != 0, "wSem unlock failed");
  }
  CERRS(pthread_mutex_unlock(&t->rSem) != 0, "rSem unlock failed");
}

void* routine(void* info)
{
  struct thInfo* thinfo = (struct thInfo*) info;
  if(isReader(thinfo->flags)) {
    int waitTime;
    DIFF(Reader(thinfo), waitTime);
    writeIfHigher(waitTime, &thinfo->readerMaxWait, &thinfo->rmSem);
  }
  if(isWriter(thinfo->flags)) {
    int waitTime;
    DIFF(Writer(thinfo), waitTime);
    writeIfHigher(waitTime, &thinfo->writerMaxWait, &thinfo->wmSem);
  }
  return NULL;
}

#define GRAIN 10 
int main(int argc, char* const argv[])
{
  int n=100, m=4, p=GRAIN; 
  int gopt;
  while((gopt = getopt(argc,argv,"n:m:p:")) != -1) {
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
    }
  }
  printf("n: %d  m: %d  p: %d\n", n,m,p);
  struct thInfo thinfo;
  memset(&thinfo, 0, sizeof(struct thInfo));
  CERRS(pthread_mutex_init(&thinfo.rSem,NULL) != 0, "sem init failed");
  CERRS(pthread_mutex_init(&thinfo.wSem,NULL) != 0, "sem init failed");
  CERRS(pthread_mutex_init(&thinfo.rmSem,NULL) != 0, "sem init failed");
  CERRS(pthread_mutex_init(&thinfo.wmSem,NULL) != 0, "sem init failed");
  pthread_t threads[n];
  pthread_attr_t pattr;
  CERRS(pthread_attr_init(&pattr), "pthread attr init failed");
  CERRS(pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_JOINABLE), "pthread attr setds failed");
  int j=0;
  struct thFlags threadFlags[n];
  for(int i=0; i < n; i++) {
    for (; j < i-m; j++) {
      CERRS(pthread_join(threads[j], NULL), "pthread join failed");
    }
    thinfo.flags = &threadFlags[i];
    if(rand() % (p+GRAIN) > GRAIN ) {
      setReader(thinfo.flags, 1);
    } else {
      setWriter(thinfo.flags, 1);
    }
    CERRS(pthread_create(&threads[i], &pattr, &routine, (void*) &thinfo), "pthread creation failed");
  }
  for (; j < n; j++) {
    CERRS(pthread_join(threads[j], NULL), "pthread join failed");
  }
  pthread_attr_destroy(&pattr);
  pthread_mutex_destroy(&thinfo.rSem);
  pthread_mutex_destroy(&thinfo.wSem);
  pthread_mutex_destroy(&thinfo.rmSem);
  pthread_mutex_destroy(&thinfo.wmSem);
  printf(" max Reader wait time: %d\n max Writer wait time: %d\n", thinfo.readerMaxWait, thinfo.writerMaxWait);
  return 0;
}
