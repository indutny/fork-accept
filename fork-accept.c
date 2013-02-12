#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <assert.h>
#include <errno.h>


struct client_state {
  int fd;
};


static int pid;


void set_nonblock(int fd) {
  int r;
  int set;

  set = 1;

  do
    r = ioctl(fd, FIONBIO, &set);
  while (r == -1 && errno == EINTR);

  assert(r == 0);
}


void handle_client(int ep, struct epoll_event* ev) {
  struct client_state* state = ev->data.ptr;
  char buf[1024];

  if (ev->events & EPOLLIN) {
    fprintf(stdout, "[%d] client data in\n", pid);
  }

  if (ev->events & EPOLLOUT) {
    fprintf(stdout, "[%d] client data out\n", pid);
  }
}


void accept_connection(int ep, int server_fd) {
  int fd;
  int r;
  struct client_state* state;
  struct epoll_event ev;

  do {
    fd = accept(server_fd, NULL, NULL);
    if (fd == -1) {
      if (errno == EAGAIN)
        return;
      else if (errno == EINTR)
        continue;
    }
  } while (0);

  set_nonblock(fd);

  fprintf(stdout, "[%d] accepted connection %d\n", pid, fd);

  /* Allocate and init client's state */
  state = malloc(sizeof(*state));
  assert(state != NULL);
  state->fd = fd;

  /* Add fd to epoll */
  ev.data.ptr = state;
  ev.events = EPOLLIN | EPOLLOUT;
  r = epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev);
  assert(r == 0);
}


void fork_child(int server_fd) {
  int i;
  int r;
  int ep;
  struct epoll_event events[100];

  r = fork();
  assert(r != -1);
  if (!r) return;

  pid = getpid();
  fprintf(stdout, "[%d] child started\n", pid);

  ep = epoll_create(1);
  assert(ep != -1);

  events[0].events = EPOLLIN;
  events[0].data.ptr = NULL;
  r = epoll_ctl(ep, EPOLL_CTL_ADD, server_fd, events);
  assert(r == 0);

  while (1) {
    r = epoll_wait(ep, events, sizeof(events) / sizeof(events[0]), -1);
    if (r == -1) {
      assert(errno == EINTR || errno == EAGAIN);
      continue;
    }

    for (i = 0; i < r; i++) {
      if (events[i].data.ptr == NULL)
        accept_connection(ep, server_fd);
      else
        handle_client(ep, &events[i]);
    }
  }
}

int main(int argc, char** argv) {
  int child_cnt;
  int port;
  int i;
  int r;
  int fd;
  struct sockaddr_in addr;

  child_cnt = 1;
  if (argc > 1) {
    child_cnt = atoi(argv[1]);
  }

  port = 8000;
  if (argc > 2) {
    port = atoi(argv[2]);
  }

  fprintf(stdout, "[%d] master started\n", getpid());

  /* Create and bind server socket */
  fd = socket(AF_INET, SOCK_STREAM, 0);
  assert(fd != -1);
  r = 1;
  r = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &r, sizeof(r));
  assert(r == 0);

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  r = bind(fd, (struct sockaddr*) &addr, sizeof(addr));
  assert(r == 0);

  /* Start accepting connections */
  r = listen(fd, 1024);
  assert(r == 0);
  set_nonblock(fd);

  /* Fork */
  for (i = 0; i < child_cnt; i++) {
    fork_child(fd);
  }

  /* Loop */
  while (1) {}

  return 0;
}
