#include "potato.h"

/* This function checks if the input is valid */
void checkInput(int argc, char ** argv);

/* This function iteratively accepts all players, stores their host
   and then send num_players, num_hops, player_id to every player. */
void accept_send_info(int listen_fd,
                      char (*host)[NI_MAXHOST],
                      uint32_t num_players,
                      uint32_t num_hops,
                      int * player_fd);

int main(int argc, char * argv[]) {
  checkInput(argc, argv);  // Check the input

  // Specify the actual length for portability
  char * port_num = argv[1];
  uint32_t num_players = atoi(argv[2]);
  uint32_t num_hops = atoi(argv[3]);
  uint32_t potato[1 + num_hops];
  potato[0] = num_hops;  // Initialize the potato
  uint32_t first_id;     // first player that received potato
  uint32_t last_id;      // last player that received potato
  int listen_fd;
  int player_fd[num_players];          // fd of all players
  char host[num_players][NI_MAXHOST];  // hostname of all players

  // Temporary placeholders
  int status;
  char player_listen_port[NI_MAXSERV];  // player's listen_port
  char signal;                          // signal from player

  /* Information about this game */

  std::cout << "Potato Ringmaster" << std::endl;
  std::cout << "Players = " << num_players << std::endl;
  std::cout << "Hops = " << num_hops << std::endl;

  /* Find a valid address with port_num, create a socket and bind */

  listen_fd = socket_and_bind(port_num);
  if (listen_fd == -1) {
    errExit("Ringmaster cannot bind to the given well-known port");
  }

  /* Listen to listen_fd */

  status = listen(listen_fd, BACKLOG);
  if (status == -1) {
    errExit("Ringmaster cannot listen to the given well-known port");
  }

  /* Iteratively accepts all players, stores their host and then
     send num_players, num_hops, player_id to every player. */

  accept_send_info(listen_fd, host, num_players, num_hops, player_fd);

  /* Receive every player's listen_port, and send every player's host and
     listen_port to its right neighbor's left_host and left_port */

  for (uint32_t i = 0; i < num_players; i++) {
    if (recv(player_fd[i], player_listen_port, sizeof(player_listen_port), 0) !=
        sizeof(player_listen_port)) {
      std::cerr << "Error: Receive player_listen_port of player " << i << std::endl;
      std::exit(EXIT_FAILURE);
    }

    uint32_t right_id = (i + 1) % num_players;

    if (send(player_fd[right_id], host[i], sizeof(host[i]), MSG_NOSIGNAL) != sizeof(host[i])) {
      std::cerr << "Error: Send host of player " << i << " to " << right_id << std::endl;
      std::exit(EXIT_FAILURE);
    }

    if (send(player_fd[right_id], player_listen_port, sizeof(player_listen_port), MSG_NOSIGNAL) !=
        sizeof(player_listen_port)) {
      std::cerr << "Error: Send listen_port of player " << i << " to " << right_id << std::endl;
      std::exit(EXIT_FAILURE);
    }
  }

  /* Receive the ready signal from every player
     Note, due to race condition, we don't know in which order the
     players is ready, but all of them will send the ready signal. */

  for (uint32_t i = 0; i < num_players; i++) {
    if (recv(player_fd[i], &signal, sizeof(signal), 0) != sizeof(signal) || signal != READY) {
      std::cerr << "Error: Receive READY signal from player " << i << std::endl;
      std::exit(EXIT_FAILURE);
    }

    std::cout << "Player " << i << " is ready to play" << std::endl;
  }

  /* If num_hops > 0, start the game and launch the potato to the first player.
     Otherwise, master and all players shut down after the ring is created. */

  if (num_hops == 0) {
    /* Close all the sockets */

    checkedClose(listen_fd);

    for (uint32_t i = 0; i < num_players; i++) {
      checkedClose(player_fd[i]);
    }

    return EXIT_SUCCESS;
  }

  /* num_hops > 0 */

  srand((unsigned int)time(NULL));
  first_id = rand() % num_players;
  std::cout << "Ready to start the game, sending potato to player " << first_id << std::endl;

  if (send(player_fd[first_id], potato, sizeof(potato), MSG_NOSIGNAL) != sizeof(potato)) {
    errExit("Ringmaster cannot send potato to the first player");
  }

  /* Wait for the returned potato from the last player "it" */

  fd_set readfds;
  FD_ZERO(&readfds);
  int maxfdp1 = 0;  // max fd + 1
  int ready;        // return value of select

  for (uint32_t i = 0; i < num_players; i++) {
    if (player_fd[i] >= FD_SETSIZE) {
      errExit("File descriptor exceeds limit FD_SETSIZE");
    }

    if (player_fd[i] >= maxfdp1) {
      maxfdp1 = player_fd[i] + 1;
    }

    FD_SET(player_fd[i], &readfds);
  }

  ready = select(maxfdp1, &readfds, NULL, NULL, NULL);
  if (ready != 1) {  // Only one player is desired to be "it"
    errExit("Ringmaster select failed");
  }

  // Get id of "it"
  last_id = 0;
  status = -1;
  for (uint32_t i = 0; i < num_players; i++) {
    if (FD_ISSET(player_fd[i], &readfds)) {
      last_id = i;
      status = 0;
      break;
    }
  }
  if (status == -1) {
    errExit("Ringmaster cannot get last_id after select");
  }

  /* Receive the potato from "it" */

  if (recv(player_fd[last_id], &potato, sizeof(potato), 0) != sizeof(potato)) {
    errExit("Ringmaster recv() \"it\" failed");
  }

  /* Output the trace */

  std::cout << "Trace of potato:" << std::endl;

  for (uint32_t i = 1; i < num_hops; i++) {
    std::cout << potato[i] << ",";
  }

  std::cout << potato[num_hops] << std::endl;

  /* Close all the sockets */

  checkedClose(listen_fd);

  for (uint32_t i = 0; i < num_players; i++) {
    checkedClose(player_fd[i]);
  }

  return EXIT_SUCCESS;
}

/* This function checks if the input is valid */
void checkInput(int argc, char ** argv) {
  if (argc == 4) {  // Otherwise, exit
    int port_num = atoi(argv[1]);
    int num_players = atoi(argv[2]);
    int num_hops = atoi(argv[3]);

    if (port_num >= 0 && port_num <= 65535 && num_players > 1 && num_hops >= 0 && num_hops <= 512) {
      return;
    }
  }

  std::cerr << "Usage: " << argv[0] << " <port_num> <num_players> <num_hops>" << std::endl;
  std::cerr << "port_num: [0, 65535], num_players > 1, num_hops: [0, 512]" << std::endl;
  std::exit(EXIT_FAILURE);
}

/* This function iteratively accepts all players, stores their host
   and then send num_players, num_hops, player_id to every player. */
void accept_send_info(int listen_fd,
                      char (*host)[NI_MAXHOST],
                      uint32_t num_players,
                      uint32_t num_hops,
                      int * player_fd) {
  int status;
  socklen_t player_addrLen = sizeof(struct sockaddr_storage);  // player's addr length
  struct sockaddr_storage player_addr;                         // player's addr info

  for (uint32_t i = 0; i < num_players; i++) {
    player_fd[i] = accept(listen_fd, (struct sockaddr *)&player_addr, &player_addrLen);
    if (player_fd[i] == -1) {
      std::cerr << "Error: Cannot accept player " << i << std::endl;
      std::exit(EXIT_FAILURE);
    }

    /* Obtain every player's host */

    status = getnameinfo(
        (struct sockaddr *)&player_addr, player_addrLen, host[i], sizeof(host[i]), NULL, 0, 0);
    if (status != 0) {
      std::cerr << "Error: Cannot hostname of player " << i << std::endl;
      std::exit(EXIT_FAILURE);
    }

    /* Translate "localhost" to the public hostname */

    if (strcmp(host[i], "localhost") == 0) {
      if (gethostname(host[i], sizeof(host[i])) == -1) {
        errExit("Cannot get ringmaster's hostname by gethostname()");
      }
    }

    /* Send num_players, num_hops and player_id to player i */

    if (send(player_fd[i], &num_players, sizeof(num_players), MSG_NOSIGNAL) !=
        sizeof(num_players)) {
      std::cerr << "Error: Send num_players to player " << i << std::endl;
      std::exit(EXIT_FAILURE);
    }

    if (send(player_fd[i], &num_hops, sizeof(num_hops), MSG_NOSIGNAL) != sizeof(num_hops)) {
      std::cerr << "Error: Send num_hops to player " << i << std::endl;
      std::exit(EXIT_FAILURE);
    }

    if (send(player_fd[i], &i, sizeof(i), MSG_NOSIGNAL) != sizeof(i)) {
      std::cerr << "Error: Send player_id to player " << i << std::endl;
      std::exit(EXIT_FAILURE);
    }
  }
}
