/********************************/
/* Authors: Mike Hale 004620459 */
/*          Edwin Do  904637634 */
/* Date:    02/18/2018          */
/* File:    server.c            */
/********************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include "sel_repeat.h"

void handle_sig(int arg);

int main(int argc, char* argv[]) {
  if (argc != 2) {
    printf("Error, expecting exactly one argument: portno\n");
    exit(1);
  }
  //signal(SIGINT, handle_sig);
  int meta_i = init_serv("", atoi(argv[1]));
  //init_summary();
  int nbyte;
  int connect;
  char buf[256];
  connect = await_connection(meta_i);
  /* Read filename */
  while((nbyte = read_sr(connect, buf, 256)) >= 0)
    if (nbyte > 0)
      break;
  buf[nbyte] = '\0';
  char* name = get_host_name(connect);
  finish_sr();
  printf("File: %s\n Contact: %s\n", buf, name);
  int send_connect = connect_rdt("127.0.0.1", 23135);
  int fd = open(buf, O_RDONLY);
  if (fd < 0)
    write_sr(send_connect, "404 Not Found\n", 14);
  else
    while ((nbyte = read(fd, buf, 256)) > 0) {
      int written_bytes = 0;
      int nbyte2;
      while(1) {
        nbyte2 = write_sr(send_connect, buf + written_bytes, nbyte - written_bytes);
        if (nbyte2 + written_bytes == nbyte)
          break;
        else
          written_bytes += nbyte2;
      }
    }
  mark_done(send_connect);

  finish_sr();
  //print_summary();
  printf("\nEnd of transmission\n");
}

void handle_sig(int arg)
{
  print_summary();
  exit(1);
}