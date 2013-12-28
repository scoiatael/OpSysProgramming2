#include "common.h"

struct Q {
  int fd;
  pid_t pid;
  struct Q* next;
}* fdQ = NULL;

char first_use = 0;

void fdQCleanUp()
{
  for(struct Q* i = fdQ; i != NULL;) {
    struct Q* n = i->next;
    free(i);
    i = n;
  }
}

void push(struct Q** start, int f, pid_t p)
{

  for(;(*start) != NULL; start = & (*start)->next) {
  }
  (*start) = (struct Q*) malloc(sizeof(struct Q));
  (*start)->next = NULL;
  (*start)->fd = f;
  (*start)->pid = p;
  if(first_use == 0) {
    atexit(&fdQCleanUp);
    first_use = 1;
  }
}

pid_t find(struct Q* start, int f)
{
  for(;start != NULL; start = start->next) {
    if(start->fd == f) {
      return start->pid;
    }
  }
  return -1;
}

int mpopen(const char* command, const char* type)
{
  int fs[2],rp;
  CERR(rp = pipe(fs), "pipe failed");
  // fs[0] -> read end
  // fs[1] -> write end
  int r;
  CERR(r = fork(), "fork failed");
  if(rp == -1 || r == -1)
  {
    exit(EXIT_FAILURE);
  }
  int of,nf;
  if(r == 0) { // Child
    if( (*type) == 'r' ) {
      nf = 1;
      of = fs[1];
    }
    if( (*type) == 'w' ) {
      nf = 0;
      of = fs[0];
    }
    if((*type) != 'w' && (*type) != 'r') {
      fprintf(stderr, "Bad mpopen arg!\n");
    }
    printf("executing %s..\n", command);
    signal(SIGHUP, &exit);
    char* SH = "/usr/bin/sh";
    dup2(of,nf);
    execl(SH, SH, "-c", command, NULL);
    // command execution continues, no return 
  } else { // Parent
    int ret;
    if( (*type) == 'r' ) {
      ret = fs[0];
    } else {
      ret = fs[1];
    }
    push(&fdQ, ret, r);
    signal(SIGCHLD, SIG_IGN);
    return ret;
  }
  return -1;
}

int mpclose(int f)
{
  pid_t p;
  CERR(p = find(fdQ, f), "file descriptor search failed");
  if(p != -1) {
    printf("\nClosing fd %d : pid %d..\n", f, p);
  }
  CERR(p = kill(p, SIGHUP), "kill failed"); 
  return p;
}

int main(int argc, const char *argv[])
{
  if(argc < 3 ) {
    printf("Usage: ./1 { -r | -w <string> } <command>");
    return EXIT_FAILURE;
  }
  int fd;
  CERR(fd = mpopen(argv[2], &argv[1][1]), "mpopen failed");
  if(strcmp(argv[1], "-r") == 0) {
    char byte;
    ssize_t ret = 1;
    while((ret = read(fd, &byte, 1)) == 1) {
      if(byte >= 'a' && byte <= 'z') {
        byte = byte + 'A' - 'a';
      }
      putchar(byte);
    }
    CERR(ret,"read failed");
    puts("Reading done.\n");
  } 
  if(strcmp(argv[1], "-w") == 0) {
    ssize_t r;
    CERR(r = write(fd, argv[3], strlen(argv[3])), "write failed");
    printf("written %d to pipe..\n", (int)r);
    system("sleep 1");
    mpclose(fd);
  }
  return 0;
}
