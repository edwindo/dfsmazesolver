/********************************/
/* Authors: Mike Hale 004620459 */
/*          Edwin Do  xx4xxxxxx */
/* Date:    02/18/2018          */
/* File:    client.c            */
/********************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "sel_repeat.h"

void handle_sig(int arg);

int main(int argc, char* argv[]) {
  if (argc != 2) {
    printf("One command line argument: Filename\n");
    exit(1);
  }
  init_summary();
  signal(SIGINT, handle_sig);
  int fd = open(argv[1], O_RDONLY);
  int meta_i = connect_rdt("127.0.0.1");
  char buf[256];
  int nchars;
  while ((nchars = read(fd, buf, 256)) > 0)
    write_sr(meta_i, buf, 256);
  mark_done(meta_i);
  finish_sr();
  print_summary();
}

void handle_sig(int arg)
{
  print_summary();
  exit(1);
}