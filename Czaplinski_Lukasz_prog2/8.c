#include "common.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#define SOCKPATH "/my_count_socket"

#define bitcount(X) (__builtin_popcount(X))

int bit_count(char* buf, int size)
{
  int sum = 0;
  for(int i = 0; i < size; i = i + 1) {
    sum += (unsigned int) bitcount((unsigned int)buf[i]);
  }
  return sum;
}

int itoa(int i, char b[])
{
  char const digit[] = "0123456789";
  char* p = b;
  int shifter = i, r = 0;
  do { //Move to where representation ends
    ++p;
    ++r;
    shifter = shifter / 10;
  } while(shifter);
  *p = '\0';
  do { //Move back, inserting digits as u go
    *--p = digit[i % 10];
    i = i / 10;
  } while(i);
  return r;
}

int frontend_s;

void closefront(int sig)
{
  if(sig == SIGINT) {
    fprintf(stderr, "Got SIGINT, ending\n");
  } else {
    fprintf(stderr, "Got %d, ending\n", sig);
  }
  close(frontend_s);
  exit(EXIT_FAILURE);
}

int startFrontend()
{
  int r = 0;
  frontend_s = socket(AF_UNIX, SOCK_STREAM, 0);
  CERR(r = frontend_s, "socket");
  if(r == -1) {
    return r;
  }
  signal(SIGINT, &closefront);
  struct sockaddr_un s_addr;
  memset(&s_addr, 0, sizeof(struct sockaddr_un));
  s_addr.sun_family = AF_UNIX;
  strncpy(s_addr.sun_path, SOCKPATH, sizeof(s_addr.sun_path) - 1);
  CERR(r = connect(frontend_s, &s_addr, sizeof(struct sockaddr_un)), "connect");
  return r;
}

#define MINSIZE 8
int sendFrontend(char* data, int* size, int maxsize)
{
  int r = 0;
  CERR(r = send(frontend_s, data, *size, 0), "send");
  if(r == -1) {
    return r;
  }
  CERR((*size) = r = recv(frontend_s, data, maxsize, 0), "rcv");
  if((unsigned int)r != sizeof(int)) {
    fprintf(stderr, "Error receiving, got %d bytes\n", (*size));
    return -1;
  } else {
    int count  = *(int*)data;
    itoa(count, data);
  }
  return r;
}

void closeFrontend()
{
  fcntl(frontend_s, F_SETFD, O_NONBLOCK);
  printf("Trying to say goodbye..\n"); 
  send(frontend_s, "0", 1, 0);
  char buf[256];
  if(recv(frontend_s, buf, 256, 0)!= -1) {
    printf(" final countdown: %d\n", *(int*)buf);
  }
  send(frontend_s, "0", 1, 0);
  close(frontend_s);
}

struct connection {
  int count;
  unsigned int  zero_count : 31;
  unsigned int send_info : 1 ;
}; 
typedef struct connection conn_t;

typedef struct pollfd pollfd_t;

#define MAXCON 50
int backend_s;
pollfd_t sockets[MAXCON];
conn_t connections[MAXCON];
unsigned char active_socket_nr;

void swap_conns(int i, int j)
{
  if(i == j) {
    return;
  }
  conn_t temp = connections[j];
  connections[j] = connections [i];
  connections[i] = temp;

  pollfd_t temp1 = sockets[j];
  sockets[j] = sockets[i];
  sockets[i] = temp1;
}

int first_free_socket()
{
  if(active_socket_nr < MAXCON) {
    return active_socket_nr++;
  }
  return -1;
}

void closesocket(int i)
{
  if(i < active_socket_nr) {
    close(sockets[i].fd);
    memset(&connections[i], 0, sizeof(conn_t));
    active_socket_nr = active_socket_nr - 1;
    if(active_socket_nr > 0) {
      swap_conns(i, active_socket_nr);
    }
  }
  printf("Closing socket %d\n", i);
}

void closeall(int sig)
{
  if(sig == SIGINT) {
    fprintf(stderr, "Got SIGINT, ending\n");
  } else {
    fprintf(stderr, "Got %d, ending\n", sig);
  }
  for(int i = active_socket_nr-1; i >= 0; i = i - 1) {
    closesocket(i);
  }
  close(backend_s);
  unlink(SOCKPATH);
  fflush(stderr);
  fflush(stdout);
  exit(EXIT_FAILURE);
}


int recvinfo(int i, char * buf, ssize_t size)
{
  int r =recv(sockets[i].fd, buf, size, 0);
  printf(" got something from %d : %d bytes\n", i, r);
  return r;
}

int send_to;
int sendinfo(int i)
{
  if(connections[i].send_info != 1) {
    return 0;
  }
  send_to = i;
  int r;
  CERR(r = send(sockets[i].fd, &connections[i].count, sizeof(int), 0), "snd");
  if(r != -1) {
    printf(" sent %d bytes to %d\n", r, i);
  }
  if(i < active_socket_nr) {
    connections[i].send_info = 0;
  }

  return 0;
}

void handle_sigpipe(int sig)
{
  if(sig != SIGPIPE) {
    return;
  }
  fprintf(stderr, "Got SIGPIPE!\n");
  closesocket(send_to);
}

void startBackend()
{
  unlink(SOCKPATH);
  backend_s = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
  CERR(backend_s, "socket");
  struct sockaddr_un s_addr;
  memset(&s_addr, 0, sizeof(struct sockaddr_un));
  s_addr.sun_family = AF_UNIX;
  strncpy(s_addr.sun_path, SOCKPATH, sizeof(s_addr.sun_path) - 1);
  CERR(bind(backend_s, &s_addr, sizeof(struct sockaddr_un)), "bind");
  struct sockaddr_un peer_addr;
  socklen_t peer_addr_size = sizeof(struct sockaddr_un);
  listen(backend_s, MAXCON);
  for(int i=0; i<MAXCON; i++) {
    sockets[i].events = POLLIN | POLLPRI | POLLOUT;
  }
  unsigned long long  quit = 1;
  signal(SIGINT, &closeall);
  signal(SIGPIPE, &handle_sigpipe);
  while(quit!=0) {
//    printf("Iteration %llu\n", quit);
    quit++;
    //accept incoming connection
    int a = accept(backend_s, &peer_addr, &peer_addr_size);
    if(a != -1) {
      printf("New connection!\n");
      int i = first_free_socket();
      if(i == -1) {
        fprintf(stderr, "Too many connections\n");
      } else {
        printf("Accepting connection at %d\n", i);
        sockets[i].fd = a;
        CERR(fcntl(a, F_SETFD, O_NONBLOCK), "fcntl");
      }
    }
    //listen for info
    CERR(poll(sockets, (int)active_socket_nr, 500), "poll");
    for (int i = 0; i < active_socket_nr; i++) {
      if(sockets[i].revents != 0) {
      //  printf("Something to do at %d\n", i);
        char buf[256];
        int size;
        if((sockets[i].revents & (POLLIN | POLLPRI)) > 0 
            && (size = recvinfo(i, buf, 256)) != -1) {
          int ones = bit_count(buf, size);
          connections[i].count = connections[i].count + ones;
          buf[size] = '\0';
          printf("  bitcount of %s : %d | from %d, total %u \n", buf, ones, i, connections[i].count);
          if(ones == 0) {
            if(connections[i].zero_count == 1) {
              printf("Goodbye %d!\n", i);
              closesocket(i);
            } else {
              connections[i].zero_count++;
              connections[i].send_info = 1;
            }
          } else {
            connections[i].send_info = 1;
          }
        } else {
          if((sockets[i].revents & (POLLOUT)) > 0) {
            sendinfo(i);
          }
        }
      }
    }
  }
}

#define BUFFERLENGTH 256
int main(int argc, char * const argv[])
{
  int r = 0, t = 0;
  char d[BUFFERLENGTH] = "";
  while((r = getopt(argc, argv, "t:d:")) != -1 ) {
    switch(r) {
    case 't':
      t = atoi(optarg);
      break;
    case 'd':
      if(strlen(optarg) > 256) {
        printf("optarg too long\n");
      } else {
        strncpy(d, optarg, 256);
      }
      break;
    }
  }
  printf(" t:%d d:%s\n", t, d);
  int i;
  switch(t) {
  case 0:
    i = strlen(d);
    if(startFrontend() == -1) {
      fprintf(stderr, "start got wrong\n");
      exit(EXIT_FAILURE);
    }
    if(i > 0) {
      printf("Send %s,", d);
      if(sendFrontend(d, &i, BUFFERLENGTH) == -1) {
        fprintf(stderr, "something went wrong\n");
        closeFrontend();
        exit(EXIT_FAILURE);
      }
      printf(" received %s\n", d);
    } else {
      printf("Strings to send:\n 'exit' line ends, buffer limit %d characters\n", BUFFERLENGTH);
      for( ;; ) {
        i = scanf("%256s", d);
        printf(" got %s\n", d);
        if(i == 0 || strcmp(d, "exit") == 0) {
          break;
        } else {
          i = strlen(d);
          printf("  sending..\n");
          if(sendFrontend(d, &i, BUFFERLENGTH) == -1) {
            fprintf(stderr, "send got wrong\n");
            closeFrontend();
            exit(EXIT_FAILURE);
          }
          printf("Received %s\n", d);

        }
      }
    }
    closeFrontend();
    break;
  case 1:
    startBackend();
    fprintf(stderr, "Server went wrong\n");
    break;
  }

  return 0;
}
