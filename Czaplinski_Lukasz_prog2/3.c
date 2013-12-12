#include "common.h"

typedef struct barrier {
  sem_t sem;
  int val;
  int cur_val;
  sem_t valSem;
} barrier_t;

#define NAME "/cbi"
#define CERRR(X,Y) {CERR(X,Y); if(X == -1) return -1; }
barrier_t* binit(unsigned int count)
{
  int fd = shm_open(NAME, O_CREAT | O_TRUNC, 999);
  CERR(fd, "shm_open failed");
  barrier_t *mem = (barrier_t*) mmap(NULL, sizeof(barrier_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  CERR((long long)mem, "mmap failed");
  if(mem == (void*)-1 || fd == -1) {
    return (void*)-1;
  }
  mem->val = count;
  mem->cur_val = 0;
  CERR(sem_init(&mem->sem, 1, 0), "sem init failed");
  CERR(sem_init(&mem->valSem, 1, 1), "sem init failed");
  return mem;
}

int bwait(barrier_t* b)
{
  CERRR(sem_wait(&b->valSem), "sem wait failed");
  b->cur_val++;
  if(b->cur_val == b->val) {
    printf("Opening barrier..");
    for (; b->cur_val >= 0; b->cur_val = b->cur_val - 1) {
      CERRR(sem_post(&b->sem), "sem post failed");
    }
    CERRR(sem_post(&b->valSem), "sem post failed");
  } else {
    CERRR(sem_post(&b->valSem), "sem post failed");
    CERRR(sem_wait(&b->sem), "sem wait failed");
  }
  return 0;
}

int bdestroy(barrier_t* b)
{
  CERRR(munmap((void*) b, sizeof(barrier_t)), "munmap failed");
  CERRR(shm_unlink(NAME), "shm unlink failed");
  return 0;
}

int main(int argc, char* const argv[])
{
  int n = 10, m = 4;
  if(getopt(argc,argv,"n:") == 'n') {
    n = atoi(optarg);
  }
  barrier_t* b = binit(n+1);
  CERRR((long long)b, "binit failed");
  int r=1;
  for (int i = 0; i < n && r > 0; i++) {
    CERR(r = fork(), "fork failed");
  }
  if(r == 0) {
    pid_t pid = getpid();
    for (int i = 0; i < m; i++) {
      bwait(b);
      delay(100,10);
      printf(" pid %d run %d\n", pid, i);
    }
  } else {
    printf("Forking done. enjoy the show!\n");
    for (int i = 0; i < m; i++) {
      bwait(b);
    }
  } 
  bdestroy(b);
  return 0;
}
