#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "string.h"
#include "stdlib.h"

#define MAX 100

int main (int argc, char* argv[])  {
  int fd;
  char buffer[MAX];
  int bytes_read;

  if ((fd = open (argv[1], O_RDONLY)) == -1) {
    perror ("erro no open");
    return -1;
  }

  while ((bytes_read = read (fd, &buffer, MAX)) > 0 ) {
    printf("%s\n",buffer);
  }

  return 0;
}
