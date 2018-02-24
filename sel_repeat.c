/********************************/
/* Authors: Mike Hale 004620459 */
/*          Edwin Do  xx4xxxxxx */
/* Date:    02/18/2018          */
/* File:    sel_repeat.c        */
/********************************/

/* Needed for usleep*/
#define _BSD_SOURCE

#include "sel_repeat.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sched.h>

#define BUF_SIZE (16*(PACK_LEN - sizeof(m_header)) + 1)
#define DATA_LEN (PACK_LEN - sizeof(m_header))
#define CONNECTION_LIMIT 8
#define THREAD_LIMIT 20

/* Buffer definitions */
#define PRE_SEQ  0
#define SENDING  1
#define COMPLETE 2

typedef struct mikes_header {
  int16_t sequence_num;
  uint16_t length;
  uint8_t SEQ : 1;
  uint8_t FIN : 1;
  uint8_t ACK : 1;
  uint8_t     : 0;
} m_header;

typedef struct header_packet {
  m_header header;
  uint8_t data[DATA_LEN];
} h_packet;

typedef struct connect_metadata {
  /* Socket variables */
  int udp_socket;
  struct sockaddr_in send_addr;

  /* Window variables */
  volatile uint16_t frame_base;
  volatile uint8_t  acked_bmap; // LSB = current packet, MSB = later packet

  /* Buffer variables */
  pthread_mutex_t buf_mutex;
  int8_t  buf_complete;
  uint16_t buf_start;
  uint16_t buf_end;
  char buffer[BUF_SIZE];
} connect_meta;

/* Function prototypes */
int connect_rdt(int port, char* hostname);
int init_serv(int port, char* hostname);
struct pth_sp_arg* make_pack_arg(int meta_i, int sequence, int length, int SEQ, int FIN, int ACK);
void* pth_send_packet(void* arg);
void route_packet(h_packet* packet, struct sockaddr_in* send_addr, int sock_fd);
int fetch_packets(int udp_socket);
int read_sr(int meta_i, void *buf, unsigned int nbyte);
int write_sr(int meta_i, void *buf, unsigned int count);
void mark_done(int meta_i);
int note_thread_id(pthread_t thr);
int remove_thread_id(pthread_t thr);
void* pth_maintain_pipe(void* arg);
void finish_sr(void);

/* Global variables */
connect_meta* meta_array[CONNECTION_LIMIT] = {0, 0, 0, 0, 0, 0, 0, 0};
pthread_t thread_id_array[THREAD_LIMIT];
uint32_t thread_bitmap = 0;
pthread_mutex_t thread_mutex;
uint8_t pipe_maintainer_thread = 0;

int connect_rdt(int port, char* hostname)
{
  /* Local variables */
  int meta_i;
  connect_meta* meta;
  pthread_t th;

  /* Allocate connect_meta structure */
  for (meta_i = 0; meta_i < CONNECTION_LIMIT; meta_i++) {
    if (meta_array[meta_i] == NULL) {
      meta_array[meta_i] = (connect_meta*)malloc(sizeof(connect_meta));
      break;
    }
    else if (meta_i == CONNECTION_LIMIT - 1)
      return -1;
  }
  meta = meta_array[meta_i];
  /* Init metadata members */
  meta->udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
  meta->send_addr.sin_family = AF_INET;
  meta->send_addr.sin_port = htons(port);
  meta->send_addr.sin_addr.s_addr = inet_addr(hostname);
  pthread_mutex_init(&meta->buf_mutex, NULL);
  pthread_mutex_lock(&meta->buf_mutex);
  meta->buf_start = 0;
  meta->buf_end = 0;
  meta->buf_complete = PRE_SEQ;
  pthread_mutex_unlock(&meta->buf_mutex);

  /* Dispatch maintainer thread */
  if (!pipe_maintainer_thread) {
    int* udp_socket = malloc(sizeof(int));
    *udp_socket = meta->udp_socket;
	pthread_mutex_init(&thread_mutex, 0);
    pthread_create(&th, 0, pth_maintain_pipe, (void*)udp_socket);
  }

  return meta_i;
}

int init_serv(int port, char* hostname)
{
  /* Local variables */
  int meta_i;
  connect_meta* meta;
  pthread_t th;
  
  /* Allocate connect_meta structure */
  for (meta_i = 0; meta_i < CONNECTION_LIMIT; meta_i++) {
    if (meta_array[meta_i] == NULL) {
      meta_array[meta_i] = (connect_meta*)malloc(sizeof(connect_meta));
      break;
    }
    else if (meta_i == CONNECTION_LIMIT - 1)
      return -1;
  }

  /* Init metadata members */
  meta = meta_array[meta_i];
  meta->udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
  meta->send_addr.sin_family = AF_INET;
  meta->send_addr.sin_port = htons(port);
  meta->send_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  meta->buf_start = 0;
  meta->buf_end = 0;
  meta->buf_complete = PRE_SEQ;

  bind(meta->udp_socket, (struct sockaddr*)&meta->send_addr,
       sizeof(struct sockaddr_in));

  /* Dispatch maintainer thread */
  if (!pipe_maintainer_thread) {
    int* udp_socket = malloc(sizeof(int));
    *udp_socket = meta->udp_socket;
    pthread_create(&th, 0, pth_maintain_pipe, (void*)udp_socket);
  }

  return meta_i;
}

struct pth_sp_arg {
  int meta_i;
  h_packet* packet;
};

struct pth_sp_arg* make_pack_arg(int meta_i, int sequence, int length, int SEQ, int FIN, int ACK)
{
  struct pth_sp_arg* pack_arg = malloc(sizeof(struct pth_sp_arg));
  h_packet* packet = malloc(sizeof(h_packet));
  packet->header.sequence_num = sequence;
  packet->header.length = length;
  packet->header.SEQ = SEQ;
  packet->header.FIN = FIN;
  packet->header.ACK = ACK;
  pack_arg->packet = packet;
  return pack_arg;
}

void* pth_send_packet(void* arg)
{
  struct pth_sp_arg* packet_arg;
  connect_meta* meta;
  note_thread_id(pthread_self());
  while (1) {
    packet_arg = (struct pth_sp_arg*)arg;
    meta = meta_array[packet_arg->meta_i];

    sendto(meta->udp_socket, packet_arg->packet, packet_arg->packet->header.length,
           0, (struct sockaddr*)&meta->send_addr, sizeof(struct sockaddr_in));
    printf("Sending packet %d %d\n", packet_arg->packet->header.sequence_num, WIN_SIZE);

    usleep(RT_TIMEOUT*1000);
    meta = meta_array[packet_arg->meta_i];
    if (meta == NULL || meta->frame_base > packet_arg->packet->header.sequence_num)
      break;
  }
  /* If this is the FIN packet, delete the metadata structure */
  if (packet_arg->packet->header.FIN) {
    meta_array[packet_arg->meta_i] = NULL;
    free(meta);
  }
  remove_thread_id(pthread_self());
  return NULL;
}

void route_packet(h_packet* packet, struct sockaddr_in* send_addr, int sock_fd)
{
  printf ("routing packet sequence: %d\n", packet->header.sequence_num);
  /* Local variables */
  int meta_i, packet_offset;
  h_packet* response;
  struct pth_sp_arg* pth_arg;
  pthread_t thr;

  //getnameinfo((struct sockaddr*)serv_addr, sizeof(serv_addr), hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST);

  /* If this is the initial SEQ packet */
  if (packet->header.SEQ && !packet->header.ACK) {
    for (meta_i = 0; meta_i < CONNECTION_LIMIT; meta_i++)
      if (meta_array[meta_i] == NULL) {
        meta_array[meta_i] = malloc(sizeof(connect_meta));
        break;
      }
      else if (meta_i == CONNECTION_LIMIT - 1)
        return;
    meta_array[meta_i]->udp_socket = sock_fd;
    meta_array[meta_i]->send_addr = *send_addr;
    meta_array[meta_i]->frame_base = packet->header.sequence_num + packet->header.length;
    meta_array[meta_i]->acked_bmap = 0;
    pthread_mutex_init(&meta_array[meta_i]->buf_mutex, NULL);
    pthread_mutex_lock(&meta_array[meta_i]->buf_mutex);
    meta_array[meta_i]->buf_start = 0;
    meta_array[meta_i]->buf_end = 0;
	meta_array[meta_i]->buf_complete = SENDING;
    pthread_mutex_unlock(&meta_array[meta_i]->buf_mutex);

    /* ACK SEQ segment */
    response = malloc(sizeof(h_packet));
    response->header.sequence_num = packet->header.sequence_num + packet->header.length;
    response->header.length = sizeof(m_header);
    response->header.SEQ = 1;
    response->header.ACK = 1;
    response->header.FIN = 0;
    
    pth_arg = malloc(sizeof(struct pth_sp_arg));
    pth_arg->meta_i = meta_i;
    pth_arg->packet = response;
    pthread_create(&thr, 0, pth_send_packet, pth_arg);

    printf("Receiving packet SYN\n");
    return;
  }

  /* Otherwise try to find matching sender */
  for (meta_i = 0; meta_i < CONNECTION_LIMIT; meta_i++) {
    if (meta_array[meta_i] != NULL) {
      if (send_addr->sin_addr.s_addr == meta_array[meta_i]->send_addr.sin_addr.s_addr)
		break;
    }
  }
  connect_meta* meta = meta_array[meta_i];

  /* If packet is out of window region */
  if (packet->header.sequence_num > meta->frame_base + WIN_SIZE - PACK_LEN)
    return;

  /* Number of packets ahead */
  packet_offset = (packet->header.sequence_num - meta->frame_base) / PACK_LEN;

  /* If this is an ACK segment */
  if (packet->header.ACK) {
    /* New ACK */
    if (packet->header.sequence_num > meta->frame_base) {
      packet_offset = (packet->header.sequence_num - meta->frame_base) / PACK_LEN;
	  meta->frame_base = packet->header.sequence_num;

      /* Add to acked bitmap */
      meta->acked_bmap |= (1 << packet_offset);

      /* Update frame_base and start sending data packets */
      while (meta->acked_bmap & 1) {
        meta->acked_bmap >>= 1;
        char send_pack_flag = 1;
        pthread_mutex_lock(&meta->buf_mutex);
        /* If there is enough data for a full packet */
        if ((meta->buf_end - meta->buf_start) % BUF_SIZE >= DATA_LEN) {
          
		  /* Determine if last packet */
		  int fin = (meta->buf_complete && (meta->buf_start + DATA_LEN) % BUF_SIZE == meta->buf_end) ? 1 : 0;
		  pth_arg = make_pack_arg(meta_i, meta->frame_base, PACK_LEN, 0, fin, 0); 
				  
          /* Copy bytes into data segment */
          for (int i = 0; i < DATA_LEN; i++)
            pth_arg->packet->data[i] = meta->buffer[(meta->buf_start + i) % BUF_SIZE];

          meta->buf_start = (meta->buf_start + DATA_LEN) % BUF_SIZE;
        } 
        /* If there is a nonzero amount of data, but we are done writing to buffer */
        else if ((meta->buf_complete == COMPLETE) && meta->buf_end != meta->buf_start) {
		  int length = (meta->buf_end - meta->buf_start) % BUF_SIZE;
          pth_arg = make_pack_arg(meta_i, meta->frame_base, length + sizeof(m_header), 0, 1, 0);

          /* Copy bytes into data segment */
          for (int i = 0; i < response->header.length; i++)
            pth_arg->packet->data[i] = meta->buffer[(meta->buf_start + i) % BUF_SIZE];

          meta->buf_start = meta->buf_end;
        }
        else send_pack_flag = 0;

        /* Unlock mutex */
        pthread_mutex_unlock(&meta->buf_mutex);
		for (int i = 0; i < pth_arg->packet->header.length - sizeof(m_header); i++)
			printf("%c", pth_arg->packet->data[i]);
        if (send_pack_flag)
          pthread_create(&thr, 0, pth_send_packet, pth_arg);

        /* Increment the frame_base */
        meta->frame_base += PACK_LEN;
      }
    }
  }

  /* If this is a data packet */
  else {   
    pthread_mutex_lock(&meta->buf_mutex);
    /* Copy packet data into circular buffer */
    for (int i = 0; i < packet->header.length - sizeof(m_header); i++) {
      /* If the buffer is too small for the data */
      if ((meta->buf_end + i + 1) % BUF_SIZE == meta->buf_start)
        return;
      meta->buffer[(meta->buf_end + i) % BUF_SIZE] = packet->data[i];
    }
	meta->buf_end += packet->header.length - sizeof(m_header);
	meta->buf_end %= BUF_SIZE;
 
    /* Mark buffer complete if this is a FIN packet */
    if (packet->header.FIN)
      meta->buf_complete = COMPLETE;

    /* Add segment to acked_bmap */
    meta->acked_bmap |= (1 << packet_offset);
	meta->acked_bmap >>= 1;
   
    /* Update buf_end for continuous data segments */
    while (meta->acked_bmap & 1) {
      meta->acked_bmap >>= 1;
      if (meta->acked_bmap) {
        meta->buf_end = (meta->buf_end + PACK_LEN - sizeof(m_header)) % BUF_SIZE;
        meta->frame_base += PACK_LEN;
      }
      else {
        meta->buf_end = (meta->buf_end + packet->header.length - sizeof(m_header)) % BUF_SIZE;
        meta->frame_base += packet->header.length;
      }
    }
    pthread_mutex_unlock(&meta->buf_mutex);
    /* Send individual ACK */
	int sequence = packet->header.sequence_num + packet->header.length;
	pth_arg = make_pack_arg(meta_i, sequence, sizeof(m_header), 0, packet->header.FIN, 1);
	
    pthread_create(&thr, 0, pth_send_packet, pth_arg);
  }
  return;
}

int fetch_packets(int udp_socket)
{
  char buf[PACK_LEN];
  struct sockaddr_in src_addr;
  socklen_t length = sizeof(struct sockaddr_in);
  while (1) {
    if(recvfrom(udp_socket, buf, PACK_LEN, MSG_DONTWAIT, (struct sockaddr*)&src_addr, &length) < 0)
      return -1;
    route_packet((h_packet*)buf, &src_addr, udp_socket);
  }
}

void* pth_maintain_pipe(void* arg)
{
  note_thread_id(pthread_self());
  pipe_maintainer_thread = 1;
  while (1) {
    fetch_packets(*(int*)arg);
    sched_yield();
  }
}

int read_sr(int meta_i, void *buf, unsigned int nbyte) 
{
  connect_meta* meta = meta_array[meta_i];
  pthread_mutex_lock(&meta->buf_mutex);
  uint16_t start = meta->buf_start;
  uint16_t end = meta->buf_end;
  char* m_buffer = meta->buffer;
  char* c_buf = (char*)buf;
  int length = (end - start) % BUF_SIZE;
  if (length != 0)
    printf("Length: %d\nBuf_start: %d\nBuf_end: %d\n", length, start, end);

  if (length == 0 && (meta->buf_complete == COMPLETE)) {
    return -1;
  }

  int bytes_read = 0;

  for (int i = 0; i < nbyte; i++) {
    if (i == length) {
      break;
    }
    c_buf[i] = m_buffer[(start+i)%BUF_SIZE];
    bytes_read++;
  }

  meta->buf_start = (start + bytes_read) % BUF_SIZE;
  pthread_mutex_unlock(&meta->buf_mutex);

  return bytes_read;

}

int write_sr(int meta_i, void *buf, unsigned int count)
{
  connect_meta* meta = meta_array[meta_i];
  pthread_mutex_lock(&meta->buf_mutex);
  uint16_t start = meta->buf_start;
  uint16_t end = meta->buf_end;
  char* m_buffer = meta->buffer;
  char* c_buf = (char*)buf;
  pthread_t thr;
  h_packet* packet;
  struct pth_sp_arg* pth_arg;

  int bytes_written = 0;

  for (int i = 0; i < count; i++) {
    int curr_byte = (end + i) % BUF_SIZE;
    if ((curr_byte + 1) % BUF_SIZE == start) {
      break;
    }
    m_buffer[curr_byte] = c_buf[i];
    bytes_written++;
  }
  meta->buf_end = (end + bytes_written) % BUF_SIZE;
  /* Send SYN segment upon initial write */
  if ((meta->buf_complete == PRE_SEQ) && ((meta->buf_end - meta->buf_start) % BUF_SIZE >= DATA_LEN)) {
	pth_arg = make_pack_arg(meta_i, 0, sizeof(m_header), 1, 0, 0);
	pthread_create(&thr, 0, pth_send_packet, pth_arg); 
  }
  /* If we didnt have enough data for a packet, but we do now! */
  else if (((meta->buf_end - meta->buf_start) % BUF_SIZE >= DATA_LEN) && 
           ((meta->buf_end - meta->buf_start) % BUF_SIZE < DATA_LEN + count)) {
    pth_arg = make_pack_arg(meta_i, 0, sizeof(h_packet), 0, 0, 0);
    for (int i = 0; i < DATA_LEN; i++) {
      pth_arg->packet->data[i] = meta->buffer[meta->buf_start];
      meta->buf_start = (meta->buf_start++) % BUF_SIZE;
    }
    pthread_create(&thr, 0, pth_send_packet, pth_arg);
  }
  for (int i = meta->buf_start; i != meta->buf_end; i = (i + 1) % BUF_SIZE)
	printf("%c", meta->buffer[i]);
  pthread_mutex_unlock(&meta->buf_mutex);

  return bytes_written;
}

void mark_done(int meta_i)
{
  connect_meta* meta = meta_array[meta_i];
  pthread_mutex_lock(&meta->buf_mutex);
  /* If we havent yet sent a packet */
  if (meta->buf_complete == PRE_SEQ) {
    struct pth_sp_arg* pack_arg = make_pack_arg(meta_i, 0, sizeof(m_header), 1, 0, 0);
	pthread_t thr;
	pthread_create(&thr, 0, pth_send_packet, pack_arg);
  }
  meta->buf_complete = COMPLETE;
  pthread_mutex_unlock(&meta->buf_mutex);
}



int note_thread_id(pthread_t thr)
{
  pthread_mutex_lock(&thread_mutex);
  uint32_t mask = 1;
  for (int i = 0; i < THREAD_LIMIT; i++) {
    if ((thread_bitmap & mask) == 0) {
      thread_id_array[i] = thr;
      thread_bitmap |= mask;
	  pthread_mutex_unlock(&thread_mutex);
      return i;
    } else
      mask <<= 1;
  }
  pthread_mutex_unlock(&thread_mutex);
  return -1;
}
  

int remove_thread_id(pthread_t thr)
{
  pthread_mutex_lock(&thread_mutex);
  uint32_t mask = 1;
  for (int i = 0; i < THREAD_LIMIT; i++) {
    if ((thread_id_array[i] == thr) && ((thread_bitmap & mask) > 0)) {
      thread_bitmap &= ~mask;
	  pthread_mutex_unlock(&thread_mutex);
      return i;
    }
    else
      mask <<= 1;
  }
  pthread_mutex_unlock(&thread_mutex);
  return -1;
}

void finish_sr(void)
{
  while(1) {
    for (int i = 0; i < CONNECTION_LIMIT; i++) {
	  if (meta_array[i] != NULL && meta_array[i]->buf_end != meta_array[i]->buf_start)
		break;
	  else if (i == CONNECTION_LIMIT - 1)
		return;
	}
    sched_yield();
  }
}