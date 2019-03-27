#ifndef POTATO_H
#define POTATO_H

#define _DEFAULT_SOURCE  // get NI_MAXHOST & NI_MAXSERV form <netdb.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#define BACKLOG 50

/* The ready signal: tells ringmaster that player is ready */
#define READY 'r'

/* This function outputs an error message and exit */
void errExit(const std::string str) {
  std::perror(str.c_str());
  std::exit(EXIT_FAILURE);
}

/* This function closes a fd and check for error */
void checkedClose(int fd) {
  if (close(fd) == -1) {
    std::cerr << "close failed" << std::endl;
  }
}

/* This function finds an avaiable address with port_num,
   and returns a socket fd which is bound to port_num */
int socket_and_bind(const char * port_num) {
  /* Get the listenable address list with port_num */

  int status;
  int listen_fd;
  struct addrinfo hints;
  struct addrinfo * addr_info_list;  // head of the addr info list

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;      // SearchingIPv4 and IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP
  hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

  status = getaddrinfo(NULL, port_num, &hints, &addr_info_list);
  if (status != 0) {
    return -1;
  }

  /* Iterate the returned address list to find one using which we
     can successfully create and bind a socket */

  struct addrinfo * curr;
  for (curr = addr_info_list; curr != NULL; curr = curr->ai_next) {
    listen_fd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
    if (listen_fd == -1) {
      continue;  // Create socket failed, try next address
    }

    // Note, here we don't set SO_REUSEADDR, because we don't want
    // different players in the same host bound to the same port.
    // Otherwise, the second player's listen() will fail.
    int yes = 1;
    status = setsockopt(listen_fd, SOL_SOCKET, 0, &yes, sizeof(yes));
    if (yes == -1) {
      freeaddrinfo(addr_info_list);
      checkedClose(listen_fd);
      return -1;
    }

    status = bind(listen_fd, curr->ai_addr, curr->ai_addrlen);
    if (status == 0) {
      break;  // bind() succeed
    }
    else {  // bind() failed, close the socket and try next address
      checkedClose(listen_fd);
    }
  }
  freeaddrinfo(addr_info_list);  // Free dynamically allocated space

  if (curr == NULL) {
    return -1;
  }

  return listen_fd;
}

/* This function finds an avaiable server address with host and port_num,
   and returns a socket fd which connects to the address */
int socket_and_connect(const char * host, const char * port_num) {
  /* Get the connectable address list with machine_name & port_num */

  int status;
  int server_fd;
  struct addrinfo hints;
  struct addrinfo * addr_info_list;  // head of the addr info list

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;      // Searching IPv4 and IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP
  hints.ai_flags = AI_NUMERICSERV;

  status = getaddrinfo(host, port_num, &hints, &addr_info_list);
  if (status != 0) {
    return -1;
  }

  /* Iterate the returned address list to find one to which we
    one successfully connect */

  struct addrinfo * curr;
  for (curr = addr_info_list; curr != NULL; curr = curr->ai_next) {
    server_fd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
    if (server_fd == -1) {
      continue;  // Create socket failed, try next address
    }

    status = connect(server_fd, curr->ai_addr, curr->ai_addrlen);
    if (status == 0) {
      break;  // connect() succeed
    }
    else {  // connect() failed, close the socket and try next address
      checkedClose(server_fd);
    }
  }
  freeaddrinfo(addr_info_list);  // Free dynamically allocated space

  if (curr == NULL) {
    return -1;
  }

  return server_fd;
}

#endif
