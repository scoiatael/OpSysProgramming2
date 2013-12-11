#ifndef COMMON_H

#define COMMON_H

#define _XOPEN_SOURCE
#include <time.h>
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

#define CERRS(X,Y) { if(X) { perror(Y); } }

#define CERR(X,Y) { CERRS((X) == -1,Y) }

#endif /* end of include guard: COMMON_H */
