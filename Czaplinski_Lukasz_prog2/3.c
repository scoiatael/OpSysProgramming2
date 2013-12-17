#include "common.h"
#define DEBUG

typedef struct barrier {
  sem_t sem;
  int val;
  int cur_val;
  pthread_mutex_t valSem;
  char name[256];
} barrier_t;

barrier_t* b;
pid_t* children;
int n;

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
    return (barrier_t*)-1;
  } 
  if(!existed) {
    errno = ENOENT;
    shm_unlink(NAME);
    return (barrier_t*)-1;
  }
  barrier_t *mem = (barrier_t*) mmap(NULL, sizeof(barrier_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  return mem;
}

barrier_t* binit(unsigned int count)
{
  int fd;
  char existed;
  if(openSharedMem(NAME, &fd, &existed) == -1) {
    return (barrier_t*)-1;
  } 
  if(existed) {
    return (barrier_t*)-1;
  }
  ftruncate(fd, sizeof(struct barrier));
  barrier_t *mem = (barrier_t*) mmap(NULL, sizeof(barrier_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  CERR((long long)mem, "mmap failed");
  if(mem == (void*)-1) { return (void*) -1; }
  mem->val = count;
  mem->cur_val = 0;
  strcpy(mem->name, NAME);
  CERR(pthread_mutex_init(&mem->valSem,NULL), "mutex init failed");
  CERR(sem_init(&mem->sem, 1, 0), "sem init failed");
  return mem;
}

int bwait(barrier_t* b)
{
  printf("  bwait..\n");
  CERR(pthread_mutex_lock(&b->valSem), "sem wait failed");
  b->cur_val++;
  printf("barrier val: %d, needed %d\n", b->cur_val, b->val);

  if(b->cur_val == b->val) {
    printf("Opening barrier..\n");
    // To avoid race during (and for) opening, valSem is left locked and opened after all posts
    // to sem.
    for (b->cur_val = b->cur_val - 1; b->cur_val > 0; b->cur_val = b->cur_val - 1) {
      CERR(sem_post(&b->sem), "sem post failed");
    }
    CERR(pthread_mutex_unlock(&b->valSem), "sem post failed");
  } else {
    CERR(pthread_mutex_unlock(&b->valSem), "sem post failed");
    //potential race: cur_val takes into account this thread, but it's not currently waiting
    CERR(sem_wait(&b->sem), "sem wait failed");
  }
  return 0;
}

void sem_unlock(sem_t* t)
{
  int r=0;
  do {
    fprintf(stderr, ".");
    fflush(stderr);
    sem_getvalue(t, &r);
    sem_post(t);
  } while(r == 0);
}

int bdestroy(barrier_t* b)
{
  pthread_mutex_destroy(&b->valSem);
  sem_destroy(&b->sem);
  char buf[256];
  strcpy(buf,b->name);
  CERR(munmap((void*) b, sizeof(struct barrier)), "munmap failed");
  CERR(shm_unlink(buf), "shm unlink failed");
  return 0;
}

void printPid(int sig __attribute__((unused)))
{
  fprintf(stderr, "pid %d closing\n", getpid()); 
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
  int t=0, n=4; 
  int opt=0;
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
      if (b == (void*)-1) { return EXIT_FAILURE; }
      printf("Initiated barrier.\n");
      break;
    case 1:
      b = bopen();
      CERR((long long)b, "bopen failed");
      if (b == (void*)-1) { return EXIT_FAILURE; }
      printf("%d started waiting..\n", getpid());
      bwait(b);
      printf("%d done waiting..\n", getpid());
      break;
    case 2:
      b = bopen();
      CERR((long long)b, "bopen failed");
      if (b == (void*)-1) { return EXIT_FAILURE; }
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
  if(getopt(argc,argv,"n:") == 'n') {
    n = atoi(optarg);
  }
  children = (pid_t*)malloc(n*sizeof(pid_t));
  b = binit(n);
  CERR((long long)b, "binit failed");
  if(b == (barrier_t*)-1) { return EXIT_FAILURE; }
  int r=1;
  for (int i = 0; i < n && r > 0; i++) {
    CERR(r = fork(), "fork failed");
    if(r!=0) {
      children[i] = r;
    }
  }
  if(r == 0) {
    pid_t pid = getpid();
    signal(SIGINT, &printPid);
    for (int i = 0; i < m; i++) {
      printf("pid %d starting %d run..\n", pid,i);
      bwait(b);
      delay(1000,10);
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
