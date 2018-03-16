/********************************/
/* Authors: Mike Hale 004620459 */
/*          Edwin Do  xx4xxxxxx */
/* Date:    02/18/2018          */
/* File:    server.c            */
/********************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "sel_repeat.h"

int main(int argc, char* argv[]) {
  int meta_i = init_serv("");
  init_summary();
  char buf[256];
  int fd = open("output.txt", O_CREAT | O_RDWR);
  if (fd < 0) {
    printf("Could not open output file\n");
    exit(1);
  }
  int nbyte;
  int connect = await_connection(meta_i);
  while((nbyte = read_sr(connect, buf, 256)) >= 0) {
    write(fd, buf, nbyte);
  }
  printf("\n");
  finish_sr();
  print_summary();
  printf("\nEnd of transmission\n");
}