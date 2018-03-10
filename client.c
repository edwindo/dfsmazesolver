/********************************/
/* Authors: Mike Hale 004620459 */
/*          Edwin Do  xx4xxxxxx */
/* Date:    02/18/2018          */
/* File:    client.c            */
/********************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "sel_repeat.h"

int main(int argc, char* argv[]) {
  init_summary();
  int meta_i = connect_rdt("127.0.0.1");
  char* test = "THIS IS A TWO HUNDRED AND FIFTY SIX CHARACTER STRING FOR TESTING OUR PROJECT REGARDING RELIABLE DATA TRANSFER IN A LOSSY NETWORK SETTING. UNFORTUNATELY I UNDERESTIMATED HOW MANY CHARACTERS TWO HUNDRED AND FIFTY SIX IS, SO NOW THIS IS JUST RAMBLING TO FILL";
  for (int i = 0; i < 10; i++)
    write_sr(meta_i, test, 256);
  mark_done(meta_i);
  finish_sr();
  print_summary();
}