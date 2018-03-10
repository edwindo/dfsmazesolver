/********************************/
/* Authors: Mike Hale 004620459 */
/*          Edwin Do  xx4xxxxxx */
/* Date:    02/18/2018          */
/* File:    server.c            */
/********************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "sel_repeat.h"

int main(int argc, char* argv[]) {
  int meta_i = init_serv("");
  char buf[256];
  int nbyte;
  int connect = await_connection(meta_i);
  while((nbyte = read_sr(connect, buf, 256)) >= 0) {
    write(1, buf, nbyte);
  }
  finish_sr();
  printf("\nEnd of transmission\n");
}