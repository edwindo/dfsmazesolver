/********************************/
/* Authors: Mike Hale 004620459 */
/*          Edwin Do  904637634 */
/* Date:    02/18/2018          */
/* File:    client.c            */
/********************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "sel_repeat.h"

void handle_sig(int arg);

int main(int argc, char* argv[]) {
  if (argc != 4) {
    printf("Three command line arguments: hostname portno filename\n");
    exit(1);
  }
  init_summary();
  signal(SIGINT, handle_sig);
  int meta_i = connect_rdt(argv[1], atoi(argv[2]));
  write_sr(meta_i, argv[3], strlen(argv[3]));
  mark_done(meta_i);
  finish_sr();
  int server = init_serv("127.0.0.1", 23135);
  printf("WHATS GOING ONE\n");
  int connect = await_connection(server);
  int nbytes;
  char buf[256];
  int fd = open("received.data", O_CREAT | O_RDWR);
  if (fd < 0) {
    printf("Error opening file\n");
    exit(1);
  }
  while ((nbytes = read_sr(connect, buf, 256)) >= 0)
    write(fd, buf, nbytes);
  finish_sr();
  print_summary();
}

void handle_sig(int arg)
{
  print_summary();
  exit(1);
}