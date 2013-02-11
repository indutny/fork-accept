#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>

void fork_child(int server_fd) {
  int r;

  r = fork();
  assert(r != -1);

  if (!r) return;

  fprintf(stdout, "[%d] child started\n", getpid());

  while (1) {}
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

  /* Fork */
  for (i = 0; i < child_cnt; i++) {
    fork_child(fd);
  }

  /* Loop */
  while (1) {}

  return 0;
}
