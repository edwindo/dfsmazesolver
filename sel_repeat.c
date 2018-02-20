/********************************/
/* Authors: Mike Hale 004620459 */
/*          Edwin Do  xx4xxxxxx */
/* Date:    02/18/2018          */
/* File:    sel_repeat.c        */
/********************************/

#include "sel_repeat.h"
#include <stdint.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>

#define BUF_SIZE 16*(PACKET_LEN - sizeof(m_header))
#define CONNECTION_LIMIT 8

typedef struct mikes_header {
  int16_t sequence_num;
  uint8_t length;
  uint8_t SEQ : 1;
  uint8_t FIN : 1;
  uint8_t ACK : 1;
  uint8_t     : 0;
} m_header;

typedef struct header_packet {
  m_header header;
  uint8_t data[PACKET_LEN - sizeof(m_header)];
} h_packet;

typedef struct connect_metadata {
  /* Socket variables */
  int udp_socket;
  struct sockaddr_in serv_addr;
  struct sockaddr_in client_addr;

  /* Window variables */
  uint16_t frame_base;
  uint8_t  acked_bmap; // LSB = current packet, MSB = later packet

  /* Buffer variables */
  uint16_t buf_start;
  uint16_t buf_end;
  char buffer[BUF_SIZE];
} connect_meta;

connect_meta* meta_array[CONNECTION_LIMIT] = {0, 0, 0, 0, 0, 0, 0, 0};

int connect_rdt(int port, char* hostname)
{
  /* Local variables */
  int meta_i;

  /* Allocate connect_meta structure */
  for (meta_i = 0; meta_i < CONNECTION_LIMIT; meta_i++) {
    if (meta_array[meta_i] == NULL)
      meta_array[meta_i] = (connect_meta*)malloc(sizeof(connect_meta));
    else if (meta_i == CONNECTION_LIMIT - 1)
      return -1;
  }

  /* Init metadata members */
  meta_array[meta_i]->udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
  meta_array[meta_i]->serv_addr.sin_family = AF_INET;
  meta_array[meta_i]->serv_addr.sin_port = htons(port);
  meta_array[meta_i]->serv_addr.sin_addr.s_addr = inet_addr(hostname);
                    
  meta_array[meta_i]->buf_start = 0;
  meta_array[meta_i]->buf_end = 0;

  return meta_i;
}

int init_serv(int port, char* hostname)
{
  /* Local variables */
  int meta_i;
  
  /* Allocate connect_meta structure */
  for (meta_i = 0; meta_i < CONNECTION_LIMIT; meta_i++) {
    if (meta_array[meta_i] == NULL);
      meta_array[meta_i] = (connect_meta*)malloc(sizeof(connect_meta));
    else if (meta_i == CONNECTION_LIMIT - 1)
      return -1;
  }

  /* Init metadata members */
  meta_array[meta_i].udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
  meta_array[meta_i].serv_addr.sin_family = AF_INET;
  meta_array[meta_i].serv_addr.sin_port = htons(porno);
  meta_array[meta_i].serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  bind(meta_array[meta_i].upd_socket, (struct sockaddr*)meta_array[meta_i].serv_addr,
       sizeof(struct sockaddr_in));

  return meta_i;
}

struct pth_sp_arg {
  int meta_i;
  h_packet* packet;
};

void* pth_send_packet(void* arg)
{
  while (1) {
    struct pth_sp_arg* packet_arg = (struct pth_sp_arg*)arg;
    connect_meta* meta = meta_array[pth_sp_arg->meta_i];

    sendto(meta->udp_socket, packet_arg->packet, packet_arg->packet->header->length,
           0, (struct sockaddr*)&meta->serv_addr, sizeof(struct sockaddr_in));
    printf("Sending packet %d %d\n", pth_sp_arg->packet->sequence_num, WIN_SIZE);

    usleep(RT_TIMEOUT*1000);
    meta = meta_array[pth_sp_arg->meta_i];
    if (meta == NULL || meta->frame_base > pth_sp_arg->packet->sequence_num)
      break;
  }
  return NULL;
}

void route_packet(h_packet* packet, struct sockaddr_in* serv_addr, int sock_fd)
{
  /* Local variables */
  int meta_i;
  h_packet response;
  struct pth_sp_arg pth_arg;
  char hbuf[NI_MAXHOST], compare[NI_MAXHOST];

  getnameinfo(serv_addr, sizeof(serv_addr), hbuf, sizeof(hbuf), NULL, NULL, NI_NUMERICHOST);

  /* If this is the initial SEQ packet */
  if (packet->header.SEQ && !packet->header.ACK) {
    for (meta_i = 0; meta_i < CONNECTION_LIMIT; meta_i++)
      if (meta_array[meta_i] == NULL)
        meta_array[meta_i] = malloc(sizeof(connection_meta));
      else if (meta_i == CONNECTION_LIMIT - 1)
        return;
    meta_array[meta_i].udp_socket = sock_fd;
    meta_array[meta_i].serv_addr = *serv_addr;
    meta_array[meta_i].frame_base = packet->header.sequence_num + packet->header.length;
    meta_array[meta_i].acked_bmap = 0;
    meta_array[meta_i].buf_start = 0;
    meta_array[meta_i].buf_end = 0;

    /* ACK SYN segment */
    response.header.sequence_num = packet->sequence_num + packet->header.length;
    response.header.length = sizeof(m_header);
    response.header.SEQ = 1;
    response.header.ACK = 1;
    response.header.FIN = 0;
    
    pth_arg.meta_i = meta_i;
    pth_arg.packet = &packet;
    pthread_t thr;
    pthread_create(&thr, 0, pth_send_packet, &pth_arg);
    printf("Receiving packet SYN\n");
    return;
  }

  /* Otherwise try to find matching sender */
  for (meta_i = 0; meta_i < CONNECTION_LIMIT; meta_i++) {
    if (meta_array[meta_i] != NULL) {
      getnameinfo(meta_array[meta_i].serv_addr, sizeof(meta_array[meta_i].serv_addr), compare, 
                  sizeof(compare), NULL, NULL, NI_NUMERICHOST);
      if (strcmp(hbuf, compare) == 0)
        break;
    }
  }
  connect_meta* meta = meta_array[meta_i];

  /* If the packet is an ACK */
  if (packet->header.ACK) {
    /* Send additional data packets if available */
    while(packet->header

  /* If packet is out of window region */
  if (packet->header.sequence_num > meta->frame_base + WIN_SIZE - PACKET_LEN)
    return;
  int packet_offset = (packet->header.sequence_num - meta->frame_base) / PACKET_LEN;

  /* Check for buffer overflow */
  int ovfl    = (meta->buf_end + packet->header.length - sizeof(m_header) + 
                 packet_offset*(PACKET_LEN - sizeof(m_header))) / BUF_SIZE;
  int new_end = (meta->buf_end + packet->header.length - sizeof(m_header) + 
                 packet_offset*(PACKET_LEN - sizeof(m_header))) % BUF_SIZE;
  if ((meta->buf_start > meta->buf_end &&
       (new_end > meta->buf_start || ovfl)) ||
      (meta->buf_end <= meta->buf_end &&
        new_buf > meta->buf_start && ovfl))
    return;

  /* Copy packet data into circular buffer */
  int nbytes_1;
  int nbytes_2;
  int offset_1;
  if (new_end >= packet->header.length - sizeof(m_header)) {
    nbytes_1 = packet->header.length - sizeof(m_header);
    nbytes_2 = 0;
    offset_1 = new_end - packet->header.length + sizeof(m_header);
  }
  else {
    nbytes_1 = packet->header.length - sizeof(m_header) - new_end;
    nbytes_2 = new_end;
    offset_1 = BUF_SIZE + new_end - packet->header.length + sizeof(m_header);
  }
  strncpy(meta->buffer + offset_1, packet->data, nbytes_1);
  if (nbytes_2 > 0)
    strncpy(meta->buffer + offset_2, packet->data + nbytes_1, nbytes_2);

  /* Add segment to acked_bmap */
  meta->acked_bmap |= (1 << packet_offset);

  /* Update buf_end for continuous data segments */
  while (meta->acked_bmap & 1) {
    meta->acked_bmap >>= 1;
    if (meta->acked_bmap) {
      meta->buf_end = (meta->buf_end + PACKET_LEN - sizeof(m_header)) % BUF_SIZE;
      meta->frame_base += PACKET_LEN;
    }
    else {
      meta->buf_end = (meta->buf_end + packet->header.length - sizeof(m_header)) % BUF_SIZE;
      meta->frame_base += packet->header.length;
    }
  }
  
  /* Send cumulative ACK */
  response.header.sequence_num = meta->frame_base;
  response.header.length = sizeof(m_header);
  response.header.SEQ = 0;
  response.header.ACK = 1;
  response.header.FIN = 0;
  return;
}

void* send_data(int meta_i, char* send_buf, int nbytes)
{
  /* Local variables */
  int byte_offset;
  connect_meta* meta = meta_array[meta_i];
  struct sockaddr_storage serv_storage;
  storage_size = sizeof(serv_storage);

  /* Send SYN segment */
  h_packet packet;
  packet.header.sequence_num = -1;
  packet.header.length = sizeof(m_header);
  packet.header.SEQ = 1;
  packet.header.ACK = 0;
  packet.header.FIN = 0;

  struct pth_sp_arg pth_arg;
  pth_arg.meta_i = meta_i;
  pth_arg.packet = &packet;


  pthread_t thr;
  pthread_create(&th, 0, pth_send_packet, &pth_arg);
  int nbytes;
  while (1) {
    nbytes = recvfrom(meta->udp_socket, packet, PACKET_LEN, 0, 
                   (struct sockaddr*)serv_storage, &storage_size);
    if (nbytes > 0) {
      if (packet.header.SEQ && packet.header.ACK && packet.header.sequence_num == -1) {
        meta->frame_base = 0;
        break;
      }
    }
    
  

  while(byte_offset < nbytes) {
    if (meta_array[meta_i].

