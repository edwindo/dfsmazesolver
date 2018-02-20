/********************************/
/* Authors: Mike Hale 004620459 */
/*          Edwin Do  xx4xxxxxx */
/* Date:    02/18/2018          */
/* File:    sel_repeat.h        */
/********************************/

#define PACK_LEN    1024
#define MAX_SEQ_NUM 30720
#define WIN_SIZE    5120
#define RT_TIMEOUT  500

int connect_rdt(int port, char* hostname);