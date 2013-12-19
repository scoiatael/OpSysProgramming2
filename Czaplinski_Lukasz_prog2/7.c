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

int main(int argc, const char *argv[])
{
  if(argc < 2) {
    printf("Specify filename as first arg\n");
    return EXIT_FAILURE;
  }
  int sv[2];
  CERR(socketpair(PF_UNIX, SOCK_DGRAM, 0, sv), "socketpair failed");
  int r;
  CERR(r = fork(), "fork1");
  if(r == -1) {
    return EXIT_FAILURE;
  }
  if(r == 0) {
    close(sv[1]);
    int fd = open(argv[1], O_RDONLY);
    ssize_t msgsize = CMSG_SPACE(sizeof(int));
    char* buf = malloc(msgsize);
    memset(buf, 0, msgsize);
    itoa(getpid(), buf);
    CERRS(write(1, buf, strlen(buf)) != (ssize_t)strlen(buf), "write failed");
    CERRS(read(fd, buf, 8) != 8, "read failed");
    CERRS(write(1, buf, 8) != 8, "write failed");
    write(1, "\n", 1);

    struct msghdr mhdr;
    memset(&mhdr, 0, sizeof(mhdr));
    mhdr.msg_controllen = msgsize;
    mhdr.msg_control = buf;

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&mhdr);
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;

    *(int*)CMSG_DATA(cmsg) = fd;

    mhdr.msg_controllen = CMSG_SPACE(sizeof(int));

    int r = 0;
    for(int i=0;r == 0 && i < 10;i++) {
      CERR(r = sendmsg(sv[0], &mhdr, 0), "sendmsg");
      printf("+");
      fflush(stdout);
    }
    printf("Sent %d out of %u data\n", r, (unsigned int)mhdr.msg_controllen);
    printf("fd: %d\n", fd);
    //close(sv[0]);
    free(buf);
  } else {
    CERR(r = fork(), "fork2");
    if(r == -1) {
      kill(r, SIGTERM);
      return EXIT_FAILURE;
    }
    if(r == 0) {
      close(sv[0]);
      struct msghdr mhdr;
      char buf[10 + CMSG_SPACE(sizeof(int))];
      memset(buf, 0, sizeof(buf));
      memset(&mhdr, 0, sizeof(mhdr));
      mhdr.msg_controllen = 256;
      mhdr.msg_control = buf;
      int r=0;
      delay(1010,1000);
      for(int i=0;r == 0 && i < 10;i++) {
        CERR(r = recvmsg(sv[1], &mhdr, 0), "recvmsg");
        printf("-");
        fflush(stdout);
      }
      printf("received %d bytes..\n", r);
      if ((mhdr.msg_flags & MSG_TRUNC) || (mhdr.msg_flags & MSG_CTRUNC)) {
        CERR(1 > 0, "msg trunc");
      }
      if(r == 0 || CMSG_FIRSTHDR(&mhdr) == NULL) {
        char error[256] = "CMSG error\n";
        write(2, error, sizeof(error));
        return EXIT_FAILURE;
      }
      int fd = CMSG_DATA(CMSG_FIRSTHDR(&mhdr))[0];
      printf("fd: %d\n", fd);
      itoa(getpid(), buf);
      CERRS(write(1, buf, strlen(buf)) != (ssize_t)strlen(buf), "write failed");
      CERRS(read(fd, buf, 8) != 8, "read failed");
      CERRS(write(1, buf, 8) != 8, "write failed");
      write(1, "\n", 1);
      //close(sv[1]);
    } else {
      int r, c, e;
      pid_t p;
      char buf[256];
      p = wait(&r);
      c = itoa(p, buf);
      buf[c] = ':';
      e = itoa(r, &buf[c + 1]);
      buf[c + e + 1] = '\n';
      CERRS(write(1, buf, c + e + 1) != (c + e + 1), "write failed");
      write(1, "\n", 1);
      p = wait(&r);
      c = itoa(p, buf);
      buf[c] = ':';
      c = itoa(r, &buf[c + 1]);
      buf[c + e + 1] = '\n';
      CERRS(write(1, buf, c + e + 1) != (c + e + 1), "write failed");
      write(1, "\n", 1);
    }
  }
  return 0;
}
