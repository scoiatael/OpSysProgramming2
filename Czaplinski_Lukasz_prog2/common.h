#ifndef COMMON_H

#define COMMON_H

#define _XOPEN_SOURCE 500
#define _BSD_SOURCE
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sys/socket.h>

#define CERRS(X,Y) { if(X) { perror(Y); } }

#define CERR(X,Y) { CERRS((X) == -1,Y) }

#define timediff(t1,t2) ((t2.tv_sec - t1.tv_sec)*1000LL+(t2.tv_usec - t1.tv_usec))

#define DIFF(X,Y) { struct timeval t1,t2; gettimeofday(&t1,NULL); X; gettimeofday(&t2,NULL); Y=timediff(t1,t2);}

#define otM (1000000000)
void delay(int r, int m)
{
  struct timespec nSleepTime;
  memset(&nSleepTime, 0, sizeof(struct timespec));
  int t = rand() % (r-m) + m;
  nSleepTime.tv_nsec = t % otM;
  nSleepTime.tv_sec = t / otM; 
  CERR(nanosleep(&nSleepTime, &nSleepTime), "nanosleep");
}

#define TRUE (1>0)
#define FALSE (1<0)

#endif /* end of include guard: COMMON_H */
