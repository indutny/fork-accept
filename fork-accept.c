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
  int ended;
  int written;
  int trailing;
  int write_off;
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


int handle_client(int ep, struct epoll_event* ev) {
  struct client_state* state = ev->data.ptr;
  const char* resp = "HTTP/1.1 200 Ok\r\n"
                     "\r\n"
                     "hello world";
  char buf[1024];
  int size;
  int r;
  int i;

  if (!state->ended && (ev->events & EPOLLIN)) {
    assert(!state->ended);
    fprintf(stdout, "[%d] client data in\n", pid);

    /* Ignore all incoming data */
    do {
      r = read(state->fd, buf, sizeof(buf));

      if (r > 0) {
        for (i = 0; i < 4 && i < r; i++)
          /* NOTE: Simplified check */
          if (buf[i] != '\r' && buf[i] != '\n')
            break;

        if (state->trailing + i >= 4)
          state->trailing = 4;
        else {
          state->trailing = 0;
          for (i = r - 1; i >= 0 && i >= r - 4; i--) {
            if (buf[i] != '\r' && buf[i] != '\n')
              break;
            state->trailing++;
          }
        }
      }
    } while (r > 0 || (r == -1 && errno == EINTR));

    if (r == 0 || state->trailing == 4) {
      state->ended = 1;
      fprintf(stdout, "[%d] client input end\n", pid);
    } else if (r == -1) {
      assert(errno == EAGAIN);
    }
  }

  if (state->ended && !state->written && (ev->events & EPOLLOUT)) {
    do {
      size = strlen(resp) - state->write_off;
      if (size == 0) {
        state->written = 1;
        fprintf(stdout, "[%d] client output end\n", pid);

        /* Remove fd from epoll */
        ev->data.ptr = state;
        ev->events = EPOLLIN | EPOLLOUT;
        r = epoll_ctl(ep, EPOLL_CTL_DEL, state->fd, ev);
        assert(r == 0);
        close(state->fd);
        return 1;
      }

      r = write(state->fd,
                resp + state->write_off,
                size);
      if (r == -1) {
        if (errno == EINTR) continue;
        assert(errno == EAGAIN);
      } else if (r == 0) {
        break;
      } else {
        fprintf(stdout, "[%d] client data out %d out of %d\n", pid, r, size);
        state->write_off += r;
      }
    } while(0);
  }

  return 0;
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
  state->ended = 0;
  state->written = 0;
  state->trailing = 0;
  state->write_off = 0;

  /* Add fd to epoll */
  ev.data.ptr = state;
  ev.events = EPOLLIN | EPOLLOUT;
  r = epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev);
  assert(r == 0);
}


void fork_child(int server_fd) {
  int i;
  int j;
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
      /* Skip removed events */
      if (events[i].data.ptr == &events[i])
        continue;

      if (events[i].data.ptr == NULL)
        accept_connection(ep, server_fd);
      else
        if (handle_client(ep, &events[i])) {
          for (j = i + 1; j < r; j++)
            if (events[i].data.ptr == events[j].data.ptr)
              events[j].data.ptr = &events[j];
          free(events[i].data.ptr);
        }
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
