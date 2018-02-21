/********************************/
/* Authors: Mike Hale 004620459 */
/*          Edwin Do  xx4xxxxxx */
/* Date:    02/18/2018          */
/* File:    client.c            */
/********************************/

#include <stdio.h>
#include <stdlib.h>
#include "sel_repeat.h"

int main(int argc, int argv[]) {
  int port;
  if (argc != 2) {
    fprintf(stderr, "Erorr\n");
    exit(1);
  }
  else
    port = atoi(argv[1]);

  int meta_i = connect_rdt(port, "127.0.0.1");
  write_sr(meta_i, "THIS IS A TEST", 15);
  mark_done(meta_i);
}