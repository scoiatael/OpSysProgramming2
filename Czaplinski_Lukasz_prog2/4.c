#include "common.h"

typedef struct cs 
{
  mqd_t queue;
} cs_t;

int cs_wait(cs_t* q)
{
  char b[256];
  CERR(mq_receive(q->queue, b, 256, NULL), "receive failed");
  return 0;
}

int cs_post(cs_t* q)
{
  CERR(mq_send(q->queue, "a", 1, 100), "send failed");
  return 0;
}

cs_t *cs_open(const char* name, unsigned val)
{
  cs_t* ob = (cs_t*) malloc(sizeof(cs_t));
  ob->queue = mq_open(name, O_RDWR | O_CREAT , S_IRWXU, NULL);
  struct mq_attr attr;
  mq_getattr(ob->queue, &attr);
  attr.mq_msgsize=32L;
  attr.mq_flags=0L;
  attr.mq_curmsgs=0L;
  mq_unlink(name);
  ob->queue = mq_open(name, O_RDWR | O_CREAT , S_IRWXU, &attr);
  CERR((long long)ob->queue, "mq open failed");
  if(ob->queue == (mqd_t)-1) {
    exit(EXIT_FAILURE);
  }
  for (unsigned i = 0; i < val; i++) {
    cs_post(ob);
  }
  return ob;
}

int cs_close(cs_t* c)
{
  mq_close(c->queue);
  free(c);
  return 0;
}

struct info {
  char type;
  cs_t* q;
};

void* routine(void* a)
{
  printf("Entering routine..\n");
  struct info* i = (struct info*)a;
  delay(100,10);
  int t;
  DIFF(
  if(i->type == 0) {
    cs_post(i->q);
  } else {
    cs_wait(i->q);
  }, t);
  printf(" done after %d.\n", t);
  return NULL;
}

int main(void)
{
  int n=10;
  int val=n;
  char buf[256];
  sprintf(buf, "/mmqcsPid%d", getpid());
  cs_t* qu = cs_open(buf, n);
  struct info in[3*n];
  pthread_attr_t pattr;
  CERRS(pthread_attr_init(&pattr), "pthread attr init failed");
  CERRS(pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_JOINABLE), "pthread attr setds failed");
  pthread_t p[3*n];
  memset(p, 0, sizeof(p));
  for (int i = 0; i < n; i++) {
    in[i].q = qu;
    if(rand() % 2 == 0) {
      printf("made poster\n");
      val = val + 1;
      in[i].type = 0;
    } else {
      printf("made receiver\n");
      val = val - 1;
      in[i].type = 1;
    }
    CERRS(0 != pthread_create(&p[i], &pattr, &routine, &in[i]), "pthread create failed");
  }
  for (int i = 0; i > val; i = i-1) {
    printf("made poster\n");
    in[n+i].type = 0;
    in[n+i].q = qu;
    CERRS(0 != pthread_create(&p[i+n], &pattr, &routine, &in[n+i]), "pthread create failed");
  }
  for (int i = 0; i < val; i ++) {
    printf("made receiver\n");
    in[2*n+i].type = 1;
    in[2*n+i].q = qu;
    CERRS(0 != pthread_create(&p[i+2*n], &pattr, &routine, &in[2*n+i]), "pthread create failed");
  }
  for (int i = 0; i < 3*n; i++) {
    if(p[i] != 0) {
      pthread_join(p[i],NULL);
    }
  }
  cs_close(qu);
  return 0;
}
