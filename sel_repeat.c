/********************************/
/* Authors: Mike Hale 004620459 */
/*          Edwin Do  xx4xxxxxx */
/* Date:    02/18/2018          */
/* File:    sel_repeat.c        */
/********************************/

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

#define BUF_SIZE (16*(PACK_LEN - sizeof(m_header)) + 1)
#define DATA_LEN (PACK_LEN - sizeof(m_header))
#define CONNECTION_LIMIT 8

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
  uint8_t data[PACK_LEN - sizeof(m_header)];
} h_packet;

typedef struct connect_metadata {
  /* Socket variables */
  int udp_socket;
  struct sockaddr_in serv_addr;
  struct sockaddr_in client_addr;

  /* Window variables */
  volatile uint16_t frame_base;
  volatile uint8_t  acked_bmap; // LSB = current packet, MSB = later packet

  /* Buffer variables */
  uint8_t  buf_complete;
  uint16_t buf_start;
  uint16_t buf_end;
  char buffer[BUF_SIZE];
} connect_meta;

connect_meta* meta_array[CONNECTION_LIMIT] = {0, 0, 0, 0, 0, 0, 0, 0};

int connect_rdt(int port, char* hostname)
{
  /* Local variables */
  int meta_i;
  connect_meta* meta;

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
  meta->serv_addr.sin_family = AF_INET;
  meta->serv_addr.sin_port = htons(port);
  meta->serv_addr.sin_addr.s_addr = inet_addr(hostname);
  
  meta->buf_start = 0;
  meta->buf_end = 0;
  meta->buf_complete = 0;

  return meta_i;
}

int init_serv(int port, char* hostname)
{
  /* Local variables */
  int meta_i;
  connect_meta* meta;
  
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
  meta->serv_addr.sin_family = AF_INET;
  meta->serv_addr.sin_port = htons(port);
  meta->serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  meta->buf_start = 0;
  meta->buf_end = 0;
  meta->buf_complete = 0;

  bind(meta->udp_socket, (struct sockaddr*)&meta->serv_addr,
       sizeof(struct sockaddr_in));

  return meta_i;
}

struct pth_sp_arg {
  int meta_i;
  h_packet* packet;
};

void* pth_send_packet(void* arg)
{
  struct pth_sp_arg* packet_arg;
  connect_meta* meta;
  while (1) {
    packet_arg = (struct pth_sp_arg*)arg;
    meta = meta_array[packet_arg->meta_i];

    sendto(meta->udp_socket, packet_arg->packet, packet_arg->packet->header.length,
           0, (struct sockaddr*)&meta->serv_addr, sizeof(struct sockaddr_in));
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
  return NULL;
}

void route_packet(h_packet* packet, struct sockaddr_in* serv_addr, int sock_fd)
{
  /* Local variables */
  int meta_i, packet_offset;
  h_packet* response;
  struct pth_sp_arg* pth_arg;
  pthread_t thr;
  char hbuf[NI_MAXHOST], compare[NI_MAXHOST];

  getnameinfo((struct sockaddr*)serv_addr, sizeof(serv_addr), hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST);

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
    meta_array[meta_i]->serv_addr = *serv_addr;
    meta_array[meta_i]->frame_base = packet->header.sequence_num + packet->header.length;
    meta_array[meta_i]->acked_bmap = 0;
    meta_array[meta_i]->buf_start = 0;
    meta_array[meta_i]->buf_end = 0;

    /* ACK SYN segment */
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
      getnameinfo((struct sockaddr*) &meta_array[meta_i]->serv_addr, sizeof(meta_array[meta_i]->serv_addr), compare, 
                  sizeof(compare), NULL, 0, NI_NUMERICHOST);
      if (strcmp(hbuf, compare) == 0)
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

      /* Add to acked bitmap */
      meta->acked_bmap |= packet_offset;

      /* Update frame_base and start sending data packets */
      while (meta->acked_bmap & 1) {
        meta->acked_bmap >>= 1;
        /* If there is enough data for a full packet */
        if ((meta->buf_end - meta->buf_start) % BUF_SIZE >= DATA_LEN) {
          /* Assemble data packet */
          response = malloc(sizeof(h_packet));
          response->header.sequence_num = meta->frame_base + WIN_SIZE;
          response->header.length = PACK_LEN;
          response->header.SEQ = 0;
          response->header.ACK = 0;
          response->header.FIN = (meta->buf_complete && (meta->buf_start + DATA_LEN) % BUF_SIZE == meta->buf_end) ? 1 : 0;
          /* Copy bytes into data segment */
          for (int i = 0; i < DATA_LEN; i++)
            response->data[i] = meta->buffer[(meta->buf_start + i) % BUF_SIZE];

          meta->buf_start = (meta->buf_start + DATA_LEN) % BUF_SIZE;

          pth_arg = malloc(sizeof(struct pth_sp_arg));
          pth_arg->meta_i = meta_i;
          pth_arg->packet = response;
          pthread_create(&thr, 0, pth_send_packet, pth_arg);
        } 
        /* If there is a nonzero amount of data, but we are done writing to buffer */
        else if (meta->buf_complete && meta->buf_end != meta->buf_start) {
          response = malloc(sizeof(h_packet));
          response->header.sequence_num = meta->frame_base + WIN_SIZE;
          response->header.length = (meta->buf_end - meta->buf_start) % BUF_SIZE;
          response->header.SEQ = 0;
          response->header.ACK = 0;
          response->header.FIN = 1;
          /* Copy bytes into data segment */
          for (int i = 0; i < response->header.length; i++)
            response->data[i] = meta->buffer[(meta->buf_start + i) % BUF_SIZE];

          meta->buf_start = meta->buf_end;

          pth_arg = malloc(sizeof(struct pth_sp_arg));
          pth_arg->meta_i = meta_i;
          pth_arg->packet = response;
          pthread_create(&thr, 0, pth_send_packet, pth_arg);
        }
        /* Increment the frame_base */
        meta->frame_base += PACK_LEN;
      }
    }
  }

  /* If this is a data packet */
  else {   
    /* Copy packet data into circular buffer */
    for (int i = 0; i < packet->header.length; i++) {
      /* If the buffer is too small for the data */
      if ((meta->buf_end + i + 1) % BUF_SIZE == meta->buf_start)
        return;
      meta->buffer[(meta->buf_end + i) % BUF_SIZE] = packet->data[i];
    }
    meta->buf_end += packet->header.length;
    meta->buf_end %= BUF_SIZE;
 
    /* Mark buffer complete if this is a FIN packet */
    if (packet->header.FIN)
      meta->buf_complete = 1;

    /* Add segment to acked_bmap */
    meta->acked_bmap |= (1 << packet_offset);
   
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

    /* Send individual ACK */
    response = malloc(sizeof(h_packet));
    response->header.sequence_num = packet->header.sequence_num;
    response->header.length = sizeof(m_header);
    response->header.SEQ = 0;
    response->header.ACK = 1;
    response->header.FIN = (packet->header.FIN);

    pth_arg = malloc(sizeof(struct pth_sp_arg));
    pth_arg->meta_i = meta_i;
    pth_arg->packet = response;
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

int read_sr(int meta_i, void *buf, unsigned int nbyte) 
{
  connect_meta* meta = meta_array[meta_i];
  fetch_packets(meta->udp_socket);
  uint16_t start = meta->buf_start;
  uint16_t end = meta->buf_end;
  char* m_buffer = meta->buffer;
  char* c_buf = (char*)buf;
  int length = (end - start) % BUF_SIZE;

  if (length == 0 && meta->buf_complete) {
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

  return bytes_read;

}

int write_sr(int meta_i, void *buf, unsigned int count)
{
  connect_meta* meta = meta_array[meta_i];
  fetch_packets(meta->udp_socket);
  uint16_t start = meta->buf_start;
  uint16_t end = meta->buf_end;
  char* m_buffer = meta->buffer;
  char* c_buf = (char*)buf;

  int bytes_written = 0;

  for (int i = 0; i < count; i++) {
    int curr_byte = (end + i) % BUF_SIZE;
    if (curr_byte == start) {
      break;
    }
    m_buffer[curr_byte] = c_buf[i];
    bytes_written++;
  }
  meta->buf_end = (end + bytes_written) % BUF_SIZE;
  return bytes_written;
}

void mark_done(int meta_i)
{
  connect_meta* meta = meta_array[meta_i];
  meta->buf_complete = 1;
}