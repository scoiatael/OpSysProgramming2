#include "common.h"
//#define DEBUG

typedef struct queue {
  pthread_mutex_t prot;
  int count;
  sem_t sem;
} queue_t;

int qinit(queue_t* q)
{
  int r1, r2, r3, r4;
  pthread_mutexattr_t p;
  CERR(r1 = pthread_mutexattr_init(&p), "attr init");
  CERR(r2 = pthread_mutexattr_setpshared(&p, PTHREAD_PROCESS_SHARED), "attr set");
  CERR(r3 = pthread_mutex_init(&q->prot, &p), "mutex init failed");
  CERR(r4 = sem_init(&q->sem, 1, 0), "sem init failed");
  q->count = 0;
  return (r1 | r2 | r3 | r4);
}

int qrelease(queue_t* q, char has_lock)
{
  int r1, r2, r3 = 0;
  if(!has_lock) {
    CERR(r1 = pthread_mutex_lock(&q->prot), "qmutex lck");
  }
  for(int i = q->count ; i > 0 && r3 == 0; i = i - 1) {
    CERR(r3 = sem_post(&q->sem), "qmutex pst");
  }
//  qreset(q, 1 > 0);
  if(!has_lock) {
    CERR(r2 = pthread_mutex_unlock(&q->prot), "qmutex ulck");
  }
  return (r1 | r2);
}

int qdestroy(queue_t* q)
{
  int r1, r2,r3;
  r3 = qrelease(q, FALSE);
  CERR(r1 = pthread_mutex_destroy(&q->prot), "qmtx dstr");
  CERR(r2 = sem_destroy(&q->sem), "qsem dstr");
  return (r1 | r2 | r3);
}

int qwait(queue_t* q)
{
  int r1, r2, r3, r4, r5;
  CERR(r1 = pthread_mutex_lock(&q->prot), "qmutex lck");
  q->count++;
  CERR(r2 = pthread_mutex_unlock(&q->prot), "qmutex ulck");
  CERR(r3 = sem_wait(&q->sem), "qsem wt");
  CERR(r4 = pthread_mutex_lock(&q->prot), "qmutex lck");
  q->count = q->count - 1;
  CERR(r5 = pthread_mutex_unlock(&q->prot), "qmutex ulck");
  return (r1 | r2 | r3 | r4 | r5);
}

int qincr(queue_t* q)
{
  int r1, r2;
  CERR(r1 = pthread_mutex_lock(&q->prot), "qmutex lck");
  q->count++;
  CERR(r2 = pthread_mutex_unlock(&q->prot), "qmutex ulck");
  return (r1 | r2);
}

int qdecr(queue_t* q)
{
  int r1, r2;
  CERR(r1 = pthread_mutex_lock(&q->prot), "qmutex lck");
  q->count = q->count - 1;
  CERR(r2 = pthread_mutex_unlock(&q->prot), "qmutex ulck");
  return (r1 | r2);
}

int qget_count(queue_t* q)
{
  int c, r1, r2;
  CERR(r1 = pthread_mutex_lock(&q->prot), "qmutex lck");
  c = q->count;
  CERR(r2 = pthread_mutex_unlock(&q->prot), "qmutex ulck");
  r1 = r1 | r2;
  return (r1 == 0) ? c : r1;
}

int qreset(queue_t* q, char has_lock)
{
  int r1, r2, r3 = 0;
  if(!has_lock) {
    CERR(r1 = pthread_mutex_lock(&q->prot), "qmutex lck");
  }
  do {
    r3 = sem_trywait(&q->sem);
  } while(r3 != -1);
  if(!has_lock) {
    CERR(r2 = pthread_mutex_unlock(&q->prot), "qmutex ulck");
  }
  return (r1 | r2);

}

typedef struct protmem {
  char* mem;
  pthread_mutex_t prot;
} protmem_t;

int protmem_init(protmem_t* pr, char* m)
{
  int r1;
  pthread_mutexattr_t p;
  CERR(pthread_mutexattr_init(&p), "attr init");
  CERR(pthread_mutexattr_setpshared(&p, PTHREAD_PROCESS_SHARED), "attr set");
  CERR(r1 = pthread_mutex_init(&pr->prot, &p), "mutex init failed");
  pr->mem = m;
  return r1;
}

int protmem_f(protmem_t* p, int (*f) (char*))
{
  int r, r1, r2;
  CERR(r1 = pthread_mutex_lock(&p->prot), "qmutex lck");
  r = (*f)(p->mem);
  CERR(r2 = pthread_mutex_unlock(&p->prot), "qmutex ulck");
  r1 = r1 | r2;
  return (r1 == 0) ? r : r1;
}

int protmem_destroy(protmem_t* p)
{
  int r;
  CERR(r = pthread_mutex_destroy(&p->prot), "protmem dstr");
  return r;
}

typedef struct barrier {
  int max_val;
  char name[256];
  char _m;
  protmem_t releasing;
  queue_t entrance;
  queue_t exit;
  queue_t critical;
} barrier_t;

barrier_t* b;
pid_t* children;
int n;
pid_t pid;


#define MAX(X,Y) ((X>Y) ? X : Y)

int openSharedMem(const char* name, int* fd, char* existed)
{
  int flags = S_IWUSR | S_IRUSR;
  (*fd) = shm_open(name, O_RDWR | O_CREAT | O_EXCL, flags);
  if((*fd) == -1) {
    if(errno == EEXIST) {
      if(existed != NULL)  {
        (*existed) = 1 > 0;
      }
      (*fd) = shm_open(name, O_RDWR, flags);
      return (*fd);
    } else {
      CERR(1 > 0, "shm_open failed");
      return (*fd);
    }
  } else {
    if(existed != NULL) {
      (*existed) = 1 < 0;
    }
    return (*fd);
  }
}

#define NAME "/my_barrier"
barrier_t* bopen()
{
  int fd;
  char existed;
  if(openSharedMem(NAME, &fd, &existed) == -1) {
    return (barrier_t*) - 1;
  }
  if(!existed) {
    errno = ENOENT;
    shm_unlink(NAME);
    return (barrier_t*) - 1;
  }
  barrier_t *mem = (barrier_t*) mmap(NULL, sizeof(barrier_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  return mem;
}

barrier_t* binit(unsigned int count)
{
  int fd;
  char existed;
  if(openSharedMem(NAME, &fd, &existed) == -1) {
    return (barrier_t*) - 1;
  }
  if(existed) {
    return (barrier_t*) - 1;
  }
  ftruncate(fd, sizeof(struct barrier));
  barrier_t *mem = (barrier_t*) mmap(NULL, sizeof(barrier_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  CERR((long long)mem, "mmap failed");
  if(mem == (void*) - 1) {
    return (void*) - 1;
  }

  CERR(qinit(&mem->entrance), "qinit entr");
  CERR(qinit(&mem->exit), "qinit exit");
  CERR(qinit(&mem->critical), "qinit critical");
  CERR(protmem_init(&mem->releasing, &mem->_m), "protmem r");

  mem->max_val = count;
  strcpy(mem->name, NAME);
  return mem;
}

int get1B(char* mem)
{
  return (int) * mem;
}

int set1BTrue(char* mem)
{
  return (*mem = TRUE);
}

int set1BFalse(char* mem)
{
  return (*mem = FALSE);
}

int bwait(barrier_t* b)
{
  while(protmem_f(&b->releasing, &get1B) == (int) TRUE ) {
    qwait(&b->entrance);
    //usleep(100);
  }

  qincr(&b->critical);

  //printf("   barrier val: %d (%d)\n", qget_count(&b->critical), pid);

  if(qget_count(&b->critical) == b->max_val) {
   // printf("..opening.. (%d)\n", getpid());
    protmem_f(&b->releasing, &set1BTrue);
    qrelease(&b->exit, FALSE);
  //  sem_unlock(&b->sem);
  } else {
    //printf(" %d on not release..\n", getpid());
    while(protmem_f(&b->releasing, &get1B) == (int) FALSE) {
      qwait(&b->exit);
      //usleep(100);
    }
  }

  //sem_unlock(&b->sem);

  qdecr(&b->critical);

  if(qget_count(&b->critical) == 0) {
    protmem_f(&b->releasing, &set1BFalse);
    qrelease(&b->entrance, FALSE);
    //sem_unlock(&b->sem);
  }
  return 0;
}

int bdestroy(barrier_t* b)
{
  CERR(qdestroy(&b->entrance), "bdstr entr");
  CERR(qdestroy(&b->exit), "bdstr exit");
  CERR(protmem_destroy(&b->releasing), "bdstr rel");
  char buf[256];
  strcpy(buf, b->name);
  CERR(munmap((void*) b, sizeof(struct barrier)), "bdstr munmap");
  CERR(shm_unlink(buf), "bdstr shm_unlink");
  return 0;
}

void printPid(int sig __attribute__((unused)))
{
  fprintf(stderr, "-->pid %d closing\n", pid);
  _exit(EXIT_FAILURE);
}

void tidyUp(int sig)
{
  fprintf(stderr, "Caught %d, clearing shared mem..\n", sig);
  for (int i = 0; i < n; i++) {
    kill(SIGINT, children[i]);
  }
  bdestroy(b);
  _exit(EXIT_FAILURE);
}

#ifdef DEBUG
int main(int argc, char* const argv[])
{
  int t = 0, n = 4;
  int opt = 0;
  while((opt = getopt(argc, argv, "t:n:")) != -1) {
    switch(opt) {
    case 't':
      t = atoi(optarg);
      break;
    case 'n':
      n = atoi(optarg);
      break;
    default:
      puts("unknown option");
      break;
    }
  }
  switch(t) {
  case 0:
    b = binit(n);
    CERR((long long)b, "binit failed");
    if (b == (void*) - 1) {
      return EXIT_FAILURE;
    }
    printf("Initiated barrier.\n");
    break;
  case 1:
    b = bopen();
    CERR((long long)b, "bopen failed");
    if (b == (void*) - 1) {
      return EXIT_FAILURE;
    }
    printf("%d started waiting..\n", getpid());
    bwait(b);
    printf("%d done waiting..\n", getpid());
    break;
  case 2:
    b = bopen();
    CERR((long long)b, "bopen failed");
    if (b == (void*) - 1) {
      return EXIT_FAILURE;
    }
    bdestroy(b);
    printf("Destroyed barrier\n");
  }
  return 0;
}
#endif




#ifndef DEBUG
int main(int argc, char* const argv[])
{
  int m = 4;
  n = 10;
  if(getopt(argc, argv, "n:") == 'n') {
    n = atoi(optarg);
  }
  children = (pid_t*)malloc(n * sizeof(pid_t));
  b = binit(n);
  CERR((long long)b, "binit failed");
  if(b == (barrier_t*) - 1) {
    return EXIT_FAILURE;
  }
  int r = 1;
  for (int i = 0; i < n && r > 0; i++) {
    CERR(r = fork(), "fork failed");
    if(r != 0) {
      children[i] = r;
    }
  }
  if(r == 0) {
    pid = getpid();
    signal(SIGINT, &printPid);
    for (int i = 0; i < m; i++) {
      bwait(b);
      delay(1000, 10);
      printf(" pid %d run %d\n", pid, i);
    }
    printPid(0);
  } else {
    signal(SIGINT, &tidyUp);
    printf("Forking done. enjoy the show!\n");
    for(int i = 0; i < n; i++) {
      int r;
      wait(&r);
    }
    tidyUp(0);
  }
  return 0;
}
#endif
