/********************************/
/* Authors: Mike Hale 004620459 */
/*          Edwin Do  904637634 */
/* Date:    02/18/2018          */
/* File:    sel_repeat.h        */
/********************************/

#ifndef SEL_REPEAT_H
#define SEL_REPEAT_H

#define PACK_LEN    1024
#define MAX_SEQ_NUM 30720
#define WIN_SIZE    1024
#define RT_TIMEOUT  500

int init_serv(char* hostname, unsigned int PORT);
int connect_rdt(char* hostname, unsigned int PORT);
int read_sr(int meta_i, void *buf, unsigned int nbyte);
int write_sr(int meta_i, void *buf, unsigned int count);
void mark_done(int meta_i);
void finish_sr(void);
int await_connection(int meta_i);
void init_summary(void);
void print_summary(void);
char* get_host_name(int meta_i);
void remove_connect(int meta_i);

#endif