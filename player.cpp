#include "potato.h"

/* This function checks if the input is valid */
void checkInput(int argc, char ** argv);

int main(int argc, char * argv[]) {
  checkInput(argc, argv);  // Check the input

  // Note, every player is the server of its right neighbor
  char signal;  // Temporarily stores signal to ringmaster
  int listen_fd, master_fd, left_fd, right_fd;
  char listen_port[NI_MAXSERV];
  char left_host[NI_MAXHOST];
  char left_port[NI_MAXSERV];
  char * port_num = argv[2];
  uint32_t num_players;
  uint32_t num_hops;
  uint32_t player_id;

  // Temporary placeholders
  int next_fd;  // The fd to which send the potato next

  /* Find an available server address with the given host and port,
     create a socket and connect to the server */

  master_fd = socket_and_connect(argv[1], port_num);
  if (master_fd == -1) {
    errExit("Cannot connect to the given host and port");
  }

  /* Receive num_players, num_hops and player_id from ringmaster */

  if (recv(master_fd, &num_players, sizeof(num_players), 0) != sizeof(num_players)) {
    errExit("Cannot recevie num_players form ringmaster");
  }

  if (recv(master_fd, &num_hops, sizeof(num_hops), 0) != sizeof(num_hops)) {
    errExit("Cannot receive num_hops from ringmaster");
  }

  if (recv(master_fd, &player_id, sizeof(player_id), 0) != sizeof(player_id)) {
    errExit("Cannot receive player_id from ringmaster");
  }

  std::cout << "Connected as player " << player_id << " out of " << num_players << " total players"
            << std::endl;

  /* Create an array to record the potato: num_hops and player_id trace */

  uint32_t potato[1 + num_hops];

  /* Set up as a server for the right neighbor (client) */

  // create and bind
  in_port_t curr_port;
  for (curr_port = 49152; curr_port <= 65535; curr_port++) {
    std::stringstream ss;
    ss << curr_port;
    ss >> listen_port;
    listen_fd = socket_and_bind(ss.str().c_str());
    if (listen_fd != -1) {
      break;
    }
  }

  if ((int)curr_port == 65536) {
    errExit("No available port left for player to bind");
  }

  // listen to right neighbor
  if (listen(listen_fd, BACKLOG) == -1) {
    errExit("listen() failed");
  }

  /* Send listen_fd to ringmaster */

  if (send(master_fd, listen_port, sizeof(listen_port), MSG_NOSIGNAL) != sizeof(listen_port)) {
    errExit("Cannot send listen_port to ringmaster");
  }

  /* Receive left_host and left_port from ringmaster */

  if (recv(master_fd, left_host, sizeof(left_host), 0) != sizeof(left_host)) {
    errExit("Cannot receive left_host form ringmaster");
  }

  if (recv(master_fd, left_port, sizeof(left_port), 0) != sizeof(left_port)) {
    errExit("Cannot receive left_port from ringmaster");
  }

  // std::cout << "Left neighbor's host: " << left_host << std::endl;

  /* Build connections with neighbors 
     Note, a player connect before its left neighbor accept,
     so after TCP 3-way handshake, the connection will be put in the
     pending queue, and then connect returns successfully.
     After that, the left neighbor can accept from the pending queue. */

  // connect to left neighbor
  left_fd = socket_and_connect(left_host, left_port);
  if (left_fd == -1) {
    errExit("Cannot connect to left_host and left_port");
  }

  // accept from right neighbor
  right_fd = accept(listen_fd, NULL, NULL);
  if (right_fd == -1) {
    errExit("accept() failed");
  }

  /* Send ready_signal to ringmaster */

  signal = READY;
  if (send(master_fd, &signal, sizeof(signal), MSG_NOSIGNAL) != sizeof(signal)) {
    errExit("Cannot send READY signal to ringmaster");
  }

  /* If num_hops == 0, shut down */

  if (num_hops == 0) {
    /* Close all the sockets */

    checkedClose(master_fd);
    checkedClose(listen_fd);
    checkedClose(left_fd);
    checkedClose(right_fd);

    return EXIT_SUCCESS;
  }

  /* If num_hops > 0, wait for the potato from ringmaster or neighbors */

  fd_set readfds;
  FD_ZERO(&readfds);
  int maxfdp1 = 0;  // max fd + 1
  int ready;        // return value of select
  int ready_fd;     // fd of the ready one

  if (master_fd >= FD_SETSIZE || left_fd >= FD_SETSIZE || right_fd >= FD_SETSIZE) {
    errExit("File descriptor exceeds limit FD_SETSIZE");
  }

  if (master_fd >= maxfdp1) {
    maxfdp1 = master_fd + 1;
  }
  FD_SET(master_fd, &readfds);

  if (left_fd >= maxfdp1) {
    maxfdp1 = left_fd + 1;
  }
  FD_SET(left_fd, &readfds);

  if (right_fd >= maxfdp1) {
    maxfdp1 = right_fd + 1;
  }
  FD_SET(right_fd, &readfds);

  srand((unsigned int)time(NULL) + player_id);

  while (1) {
    // Note, if the other end of the socket is closed, it will
    // be counted as valid for select - read return 0 (EOF) immediately.
    fd_set new_set = readfds;
    ready = select(maxfdp1, &new_set, NULL, NULL, NULL);
    if (ready == -1) {
      errExit("select() failed");
    }

    // Get the fd that is ready
    if (FD_ISSET(master_fd, &new_set)) {  // first priority (terminating signal)
      ready_fd = master_fd;
    }
    else if (FD_ISSET(left_fd, &new_set)) {
      ready_fd = left_fd;
    }
    else if (FD_ISSET(right_fd, &new_set)) {
      ready_fd = right_fd;
    }
    else {
      errExit("Cannot get the ready fd");
    }

    // Receive the potato
    ssize_t numBytes;
    if ((numBytes = recv(ready_fd, potato, sizeof(potato), 0)) != sizeof(potato)) {
      if (numBytes == 0) {  // EOF: fd closed, then shut down
        break;
      }
      else {
        errExit("recv failed");
      }
    }

    /* Decrement potato[0] and launch to next one */

    potato[0]--;
    potato[num_hops - potato[0]] = player_id;  // append player_id
    if (potato[0] == 0) {
      std::cout << "I'm it" << std::endl;
      next_fd = master_fd;
    }
    else {
      if (rand() % 2 == 0) {
        next_fd = left_fd;
        std::cout << "Sending potato to " << (player_id + num_players - 1) % num_players
                  << std::endl;
      }
      else {
        next_fd = right_fd;
        std::cout << "Sending potato to " << (player_id + 1) % num_players << std::endl;
      }
    }

    if (send(next_fd, potato, sizeof(potato), MSG_NOSIGNAL) != sizeof(potato)) {
      errExit("send potato failed");
    }
  }

  /* Close all the sockets */

  checkedClose(master_fd);
  checkedClose(listen_fd);
  checkedClose(left_fd);
  checkedClose(right_fd);

  return EXIT_SUCCESS;
}

/* This function checks if the input is valid */
void checkInput(int argc, char ** argv) {
  if (argc == 3) {  // Otherwise, exit
    int port_num = atoi(argv[2]);

    if (port_num >= 0 && port_num <= 65535) {
      return;
    }
  }

  std::cerr << "Usage: " << argv[0] << " <machine_name> <port_num>" << std::endl;
  std::cerr << "port_num: [0, 65535]" << std::endl;
  std::exit(EXIT_FAILURE);
}
