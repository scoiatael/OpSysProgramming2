#include "common.h"

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

void print_int(int i, char* buf)
{
  itoa(i,buf);
  CERRS(write(1, buf, strlen(buf)) != (ssize_t)strlen(buf), "write failed");
}

void print_str(const char* buf)
{
  write(1, buf, strlen(buf));
}

int backend(const char* fn, int backend_fd)
{
  int fd = open(fn, O_RDONLY);
  ssize_t msgsize = CMSG_SPACE(sizeof(int));
  char buf[msgsize + 64];

  print_str("backend (pid: ");
  print_int(getpid(), buf);
  print_str(") read: ");
  CERRS(read(fd, buf, 8) != 8, "read failed");
  CERRS(write(1, buf, 8) != 8, "write failed");
  print_str("\n");

  memset(buf, 0, msgsize);

  struct iovec iov;
  iov.iov_base = "OK";
  iov.iov_len = 2;

  struct msghdr mhdr;
  memset(&mhdr, 0, sizeof(struct msghdr));
  mhdr.msg_iov = &iov;
  mhdr.msg_iovlen = 1;
  mhdr.msg_control = buf;
  mhdr.msg_controllen = msgsize;

  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&mhdr);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));
  *(int*)CMSG_DATA(cmsg) = fd;


  mhdr.msg_controllen = cmsg->cmsg_len;

  int ret = 0;
  CERR(ret = sendmsg(backend_fd, &mhdr, 0), "sendmsg");
  return ret;
}

int frontend(int frontend_fd)
{
  char control[10 + CMSG_SPACE(sizeof(int))], data[64];
  memset(data, 0, sizeof(data));
  memset(control, 0, sizeof(control));

  struct iovec iov;
  iov.iov_base = data;
  iov.iov_len = sizeof(data)-1;

  struct msghdr mhdr;
  memset(&mhdr, 0, sizeof(mhdr));
  mhdr.msg_iov = &iov;
  mhdr.msg_iovlen = 1;
  mhdr.msg_control = control;
  mhdr.msg_controllen = sizeof(control);

  int ret = 0;
  CERR(ret = recvmsg(frontend_fd, &mhdr, 0), "recvmsg");

  if ((mhdr.msg_flags & MSG_TRUNC) || (mhdr.msg_flags & MSG_CTRUNC)) {
    CERR(1 > 0, "msg trunc");
  }

  if(ret == 0 || CMSG_FIRSTHDR(&mhdr) == NULL) {
    char error[256] = "CMSG error\n";
    write(2, error, sizeof(error));
    return EXIT_FAILURE;
  }
  int fd = CMSG_DATA(CMSG_FIRSTHDR(&mhdr))[0];

  print_str("frontend (pid: ");
  print_int(getpid(), data);
  print_str(") read: ");
  CERRS(read(fd, data, 8) != 8, "read failed");
  CERRS(write(1, data, 8) != 8, "write failed");
  print_str("\n");

  return ret;
}


int main(int argc, char* const argv[])
{
  if(argc < 2) {
    printf("Specify filename as first arg\n");
    return EXIT_FAILURE;
  }
  int sv[2];
  CERR(socketpair(AF_UNIX, SOCK_DGRAM, 0, sv), "socketpair failed");
  int r;
  CERR(r = fork(), "fork1");
  if(r == -1) {
    return EXIT_FAILURE;
  }
  if(r == 0) {
    close(sv[1]);
    backend(argv[1], sv[0]);
    close(sv[0]);
  } else {
    CERR(r = fork(), "fork2");
    if(r == -1) {
      kill(r, SIGTERM);
      return EXIT_FAILURE;
    }
    if(r == 0) {
      close(sv[0]);
      frontend(sv[1]);
      close(sv[1]);
    } else {
      int r;
      pid_t p;
      char buf[256];
      p = wait(&r);
      print_str("child ");
      print_int(p, buf);
      print_str(" returned ");
      print_int(r, buf);
      print_str("\n");

      p = wait(&r);
      print_str("child ");
      print_int(p, buf);
      print_str(" returned ");
      print_int(r, buf);
      print_str("\n");
    }
  }
  return 0;
}
