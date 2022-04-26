#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

class Message {
public:
  int offset;
  char buf[1024];
  int len;
};

enum ConnType { client, server };
class Connection {
public:
  int fd = -1;
  int left = -1;
  int right = -1;

  void print() { printf("fd: %d, left: %d, right: %d\n", fd, left, right); }
};

void *relay(void *arg) {
  // get arg
  int *arg_int = (int *)arg;
  printf("%s: %d\n", __FUNCTION__, *arg_int);
  char tcp_buf[1024];
  int ret_i = 0;

  // add it to epoll
  int epfd = epoll_create(1);
  if (epfd < 0) {
    printf("%s: ", __FUNCTION__);
    perror("epoll_create");
    return NULL;
  }
  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLET;

  // use events ptr to store corrspondent socket fd

  // create a tcp server
  int tcp = socket(AF_INET, SOCK_STREAM, 0);
  if (tcp < 0) {
    printf("%s: ", __FUNCTION__);
    perror("socket");
    return NULL;
  }
  sockaddr_in addr_tcp;
  memset(&addr_tcp, 0, sizeof(addr_tcp));
  addr_tcp.sin_family = AF_INET;
  addr_tcp.sin_port = htons(50000 + *arg_int);
  addr_tcp.sin_addr.s_addr = htonl(INADDR_ANY);
  ret_i = bind(tcp, (sockaddr *)&addr_tcp, sizeof(addr_tcp));
  if (ret_i < 0) {
    printf("%s: ", __FUNCTION__);
    perror("tcp bind");
    return NULL;
  }
  ret_i = listen(tcp, 5);
  if (ret_i < 0) {
    printf("%s: ", __FUNCTION__);
    perror("listen");
    return NULL;
  }
  // add it to epoll
  ev.events = EPOLLIN | EPOLLET;
  // ev.data.fd = tcp;
  // create connection
  ev.data.fd = tcp;
  ret_i = epoll_ctl(epfd, EPOLL_CTL_ADD, tcp, &ev);
  if (ret_i < 0) {
    printf("%s: ", __FUNCTION__);
    perror("epoll_ctl");
    return NULL;
  }
  struct epoll_event events[10];

  // whenever client mode or server mode, should use connected fd to communicate
  while (1) {
    // epoll wait on events
    int nfds = epoll_wait(epfd, events, 10, -1);
    if (nfds < 0) {
      printf("%s: ", __FUNCTION__);
      perror("epoll_wait");
      break;
    }
    for (int i = 0; i < nfds; i++) {
      printf("trigger\n");

      if (events[i].data.fd == tcp) {
        // we have a new connection
        socklen_t len = sizeof(addr_tcp);
        int client_conn = accept(tcp, (sockaddr *)&addr_tcp, &len);
        if (client_conn < 0) {
          printf("%s: ", __FUNCTION__);
          perror("accept");
          break;
        }
        // set it as non-blocking
        int flags = fcntl(client_conn, F_GETFL, 0);
        if (flags < 0) {
          printf("%s: ", __FUNCTION__);
          perror("fcntl");
          break;
        }
        flags |= O_NONBLOCK;
        ret_i = fcntl(client_conn, F_SETFL, flags);
        if (ret_i < 0) {
          printf("%s: ", __FUNCTION__);
          perror("fcntl");
          break;
        }
        Connection *left_conn = new Connection();
        left_conn->left = client_conn;
        left_conn->fd = client_conn;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.ptr = left_conn;
        ret_i = epoll_ctl(epfd, EPOLL_CTL_ADD, client_conn, &ev);
        if (ret_i < 0) {
          printf("%s: ", __FUNCTION__);
          perror("epoll_ctl");
          break;
        }

        printf("%s: accept from %d\n", __FUNCTION__, *arg_int);

        // connect to server in 172.16.144.1
        int tcp_client = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_client < 0) {
          printf("%s: ", __FUNCTION__);
          perror("socket");
          break;
        }
        // set socket to non-blocking
        flags = fcntl(tcp_client, F_GETFL, 0);
        if (flags < 0) {
          printf("%s: ", __FUNCTION__);
          perror("fcntl");
          break;
        }
        flags |= O_NONBLOCK;
        ret_i = fcntl(tcp_client, F_SETFL, flags);
        if (ret_i < 0) {
          printf("%s: ", __FUNCTION__);
          perror("fcntl");
          break;
        }
        sockaddr_in addr_tcp_client;
        memset(&addr_tcp_client, 0, sizeof(addr_tcp_client));
        addr_tcp_client.sin_family = AF_INET;
        addr_tcp_client.sin_port = htons(50000 + *arg_int);
        addr_tcp_client.sin_addr.s_addr = inet_addr("172.16.144.1");
        ret_i = connect(tcp_client, (sockaddr *)&addr_tcp_client,
                        sizeof(addr_tcp_client));
        if (ret_i < 0 && errno != EINPROGRESS) {
          printf("%s: ", __FUNCTION__);
          perror("connect");
          break;
        }
        if (ret_i == 0) {
          printf("%s: connect to server %d success\n", __FUNCTION__, *arg_int);
          // add it to epoll
          ev.events = EPOLLIN | EPOLLET;
          Connection *conn = new Connection();
          conn->fd = tcp_client;
          conn->left = client_conn;
          conn->right = tcp_client;
          left_conn->right = tcp_client;
          ev.data.ptr = conn;
          ret_i = epoll_ctl(epfd, EPOLL_CTL_ADD, tcp_client, &ev);
          if (ret_i < 0) {
            printf("%s: ", __FUNCTION__);
            perror("epoll_ctl");
            break;
          }
        } else {
          printf("%s: connect to server %d in progress\n", __FUNCTION__,
                 *arg_int);
          // add it to epoll
          Connection *tcp_client_conn = new Connection();
          tcp_client_conn->fd = tcp_client;
          tcp_client_conn->left = client_conn;
          tcp_client_conn->right = tcp_client;
          left_conn->right = tcp_client;
          ev.events = EPOLLOUT | EPOLLET;
          ev.data.ptr = tcp_client_conn;
          ret_i = epoll_ctl(epfd, EPOLL_CTL_ADD, tcp_client, &ev);
          if (ret_i < 0) {
            printf("%s: ", __FUNCTION__);
            perror("epoll_ctl");
            break;
          }
        }
        continue;
      }
      printf("here\n");
      Connection *conn = static_cast<Connection *>(events[i].data.ptr);
      conn->print();

      switch (events[i].events) {
      case EPOLLIN: {
        // read from client
        printf("epoll in\n");
        char buf[1024];
        int ret_i = recv(conn->fd, buf, sizeof(buf), 0);
        if (ret_i < 0) {
          printf("%s: ", __FUNCTION__);
          perror("recv");
          break;
        }
        printf("ready to send");

        // check current fd is left or right
        if (conn->fd == conn->left) {
          // send to right
          ret_i = send(conn->right, buf, ret_i, 0);
          if (ret_i < 0) {
            printf("%s: ", __FUNCTION__);
            perror("send");
            break;
          }
          printf("send to right\n");
        } else {
          // send to left
          ret_i = send(conn->left, buf, ret_i, 0);
          if (ret_i < 0) {
            printf("%s: ", __FUNCTION__);
            perror("send");
            break;
          }
          printf("send to left\n");
        }

        break;
      }
      case EPOLLOUT: {

        // we finally connect to server, do nothing, since we already setup the
        // connection
        // we chenge the event to EPOLLIN
        epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.ptr = conn;
        ret_i = epoll_ctl(epfd, EPOLL_CTL_MOD, conn->right, &ev);
        if (ret_i < 0) {
          printf("%s: ", __FUNCTION__);
          perror("epoll_ctl");
          break;
        }
        printf("ready to wait for EPOLLIN\n");
        break;
      }
      default: {
        printf("events not matching %d\n", events[i].events);
        break;
      }
      }
    }
  }
  return NULL;
}

int main() {

  pthread_t relay7;
  int relay_id_7 = 7;
  pthread_create(&relay7, NULL, relay, &relay_id_7);

  // wait for the thread to exit
  pthread_join(relay7, NULL);
}
