#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void fatal(int i) { 
    printf("FATAL %d\n", i);
    exit(-1); }

#define SIZE 65356
#define SIZE4 261424
#define EMPTY -1

typedef struct SocketList_tag {
  int size;
  int c[SIZE];
} SocketList;

void SocketList_init(SocketList* sl) {
  sl->size = 0;
  memset(sl->c, EMPTY, SIZE);
}

void SocketList_add(SocketList* sl, int fd, int id) {
  if (sl->c[fd] == EMPTY) {
    sl->c[fd] = id;
    ++(sl->size);
  }
}

void SocketList_remove(SocketList* sl, int fd) {
  if (sl->c[fd] != EMPTY) {
    sl->c[fd] = EMPTY;
    --(sl->size);
  }
}

void send_to_all(SocketList* sl, int fd, fd_set* writes, char str[SIZE]) {
  for (int j = sl->size, i = 0; j > 0; ++i) {
    if (sl->c[i] != EMPTY) {
      --j;
      if (i != fd && FD_ISSET(i, writes)) {
        if (send(i, str, strlen(str), MSG_DONTWAIT) < 0) {
          fatal(1);
        }
      }
    }
  }
}

void join_client_info(SocketList* sl, int fd, fd_set* writes) {
  char msg[SIZE];
  memset(msg, 0, SIZE);
  sprintf(msg, "server: client %d just arrived\n", sl->c[fd]);
  send_to_all(sl, fd, writes, msg);
}

void left_client_info(SocketList* sl, int fd, fd_set* writes) {
  char msg[SIZE];
  memset(msg, 0, SIZE);
  sprintf(msg, "server: client %d just left\n", sl->c[fd]);
  send_to_all(sl, fd, writes, msg);
}

void send_message(SocketList* sl, int fd, fd_set* writes, char str[SIZE4]) {
  char msg[SIZE];
  memset(msg, 0, SIZE);
  char tmp[SIZE];
  memset(tmp, 0, SIZE);
  for (size_t i = 0, j = 0; i < strlen(str); ++i) {
    if (str[i] == '\n') {
      sprintf(msg, "client %d: %s\n", sl->c[fd], tmp);
      send_to_all(sl, fd, writes, msg);
      memset(msg, 0, SIZE);
      memset(tmp, 0, SIZE);
      j = 0;
    } else {
      tmp[j++] = str[i];
    }
  }
}

int main(int argc, char** argv) {
  if (argc != 2) fatal(2);

  int servFd = socket(AF_INET, SOCK_STREAM, 0);
  if (servFd < 0) fatal(3);

  struct sockaddr_in servaddr;
  servaddr.sin_port = htons(atoi(argv[1]));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(2130706433);
  if (bind(servFd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) fatal(5);
  if (listen(servFd, 10) < 0) fatal(6);

  fd_set saved, reads, writes;
  int clientId = 0;
  SocketList sl;
  SocketList_init(&sl);
  while (1) {
    FD_ZERO(&saved);
    FD_SET(servFd, &saved);
    int maxFd = servFd;
    for (int j = sl.size, i = 0; j > 0; ++i) {
      if (sl.c[i] != EMPTY) {
        FD_SET(i, &saved);
        if (i > maxFd) maxFd = i;
        --j;
      }
    }
    reads = writes = saved;
    if (select(maxFd + 1, &reads, &writes, NULL, 0) < 0) fatal(7);

    if (FD_ISSET(servFd, &reads)) {
      struct sockaddr_in cli;
      int len = sizeof(cli);
      int clientFd = accept(servFd, (struct sockaddr*)&cli, (socklen_t*)&len);
      if (clientFd < 0) fatal(8);
      SocketList_add(&sl, clientFd, clientId++);
      join_client_info(&sl, clientFd, &writes);
    }

    for (int j = sl.size, i = 0; j > 0; ++i) {
      if (sl.c[i] != EMPTY) {
        if (FD_ISSET(i, &reads)) {
          char recv_msg[SIZE4];
          memset(recv_msg, 0, SIZE4);
          int result = recv(i, recv_msg, SIZE4, 0);
          if (result <= 0) {
            left_client_info(&sl, i, &writes);
            SocketList_remove(&sl, i);
            close(i);
          } else {
            send_message(&sl, i, &writes, recv_msg);
          }
        }
        --j;
      }
    }
  }
}
