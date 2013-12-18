#include "common.h"

#define CUST 2

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

typedef struct resturant {
  sem_t entrance;
  int eating;
  int waiting;
  pthread_mutex_t accessVars;
} restaurant_t;

int restaurant_init(restaurant_t* r) 
{
  int ret=0;
  r->eating = 0;
  r->waiting = 0;
  ret=sem_init(&r->entrance, 1, 0);
  if(ret == 0) {
    ret = pthread_mutex_init(&r->accessVars, NULL);
  } 
  return ret;
}

int restaurant_destroy(restaurant_t* r)
{
  int ret = sem_destroy(&r->entrance);
  if(ret != -1) {
    ret = pthread_mutex_destroy(&r->accessVars);
  } else {
    pthread_mutex_destroy(&r->accessVars);
  }
  return ret;
}

void start_eating(restaurant_t* place)
{
  CERR(pthread_mutex_lock(&place->accessVars), "lck");
  printf("%d customers at table..\n", place->eating);
  if(place->eating >= CUST) {
    place->waiting++;
    CERR(pthread_mutex_unlock(&place->accessVars), "ulck");
    //race cond between unlocking and waiting on sem
    printf("waiting for turn..\n");
    CERR(sem_wait(&place->entrance), "wait");
    CERR(pthread_mutex_lock(&place->accessVars), "lck");
    place->waiting = place->waiting - 1;
  }
  place->eating++;
  CERR(pthread_mutex_unlock(&place->accessVars), "ulck");
}

#define MIN(X,Y) ((X<Y) ? X : Y)
void end_eating(restaurant_t* place)
{
  CERR(pthread_mutex_lock(&place->accessVars), "lck");
  place->eating = place->eating - 1;
  if(place->eating == 0 && place->waiting > 0) {
    printf("Posting..\n");
    for(int i = 0; i < MIN(CUST, place->waiting); i++) {
      CERR(sem_post(&place->entrance), "post");
    }
  }
  CERR(pthread_mutex_unlock(&place->accessVars), "ulck");
}

#define NAME "/ramen"

int main(int argc, char *const argv[])
{
  int ret = 0, t=1000,A=0;
  while((ret = getopt(argc, argv, "t:A")) != -1 ) {
    switch(ret) {
      case 't':
        t = atoi(optarg);
        break;
      default:
        fprintf(stderr, "unknown option: %c\n", ret);
        break;
      case 'A':
        A = 1;
        printf("Unlinking memory\n");
    }
  }
  if(A == 1) {
    shm_unlink(NAME);
    return EXIT_SUCCESS;
  }
  int fd;
  char existed;
  CERR(openSharedMem(NAME, &fd, &existed), "oSM");
  if(fd == -1) {
    return EXIT_FAILURE;
  }
  if(!existed) {
    ftruncate(fd, sizeof(restaurant_t));
  }
  restaurant_t* r = (restaurant_t*)mmap(NULL, sizeof(restaurant_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  CERR((long long)r, "mmap");
  if(r == (restaurant_t*)-1) {
    shm_unlink(NAME);
    return EXIT_FAILURE;
  }
  if(!existed) {
    printf("initializing shm..\n");
    restaurant_init(r);
  }
  start_eating(r);
  printf(" p%d started eating..\n", (int)getpid());
  char buf[256];
  sprintf(buf,"sleep %d", t);
  system(buf);
  end_eating(r);
  printf(" p%d ended eating..\n", (int)getpid());
  return 0;
}
