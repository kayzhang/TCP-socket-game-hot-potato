# TCP-socket-game-hot-potato

Compile the source code:

```bash
$ make
```

Run the server:

```bash
$ ./ringmaster <port_num> <num_players> <num_hops>
```

Here, port_num: [0, 65535], num_players > 1, num_hops: [0, 512]

Run the client:

```bash
$ ./player <machine_name> <port_num>
```

Here, port_num: [0, 65535]                              