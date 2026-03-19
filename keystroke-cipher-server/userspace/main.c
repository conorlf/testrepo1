#include <stdio.h>
  #include <pthread.h>
  #include <signal.h>
  #include "network/server.h"
  #include "network/peer_manager.h"
  #include "messaging/direct.h"
  #include "messaging/chatroom.h"

  static volatile int running = 1;

  static void signal_handler(int sig)
  {
      running = 0;
      server_stop();
      peer_manager_cleanup();
  }

  int main(int argc, char *argv[])
  {
      pthread_t direct_thread, chatroom_thread;

      printf("KeyCipher daemon starting...\n");

      //register signal handlers
      signal(SIGINT,  signal_handler);
      signal(SIGTERM, signal_handler);

      //load peer list
      if (peer_manager_init("../peers.conf") < 0) {
          fprintf(stderr, "Failed to load peers.conf\n");
          return 1;
      }
      //start HTTPS server for incoming messages
      if (server_init(8443) < 0) {
          fprintf(stderr, "Failed to start server\n");
          return 1;
      }
      //connect to all peers
      peer_manager_connect_all();

      //thread: blocking reads from /dev/keycipher_in
      pthread_create(&direct_thread, NULL, direct_receive_loop, NULL);
      pthread_detach(direct_thread);

      //thread: blocking reads from chatroom FIFO
      pthread_create(&chatroom_thread, NULL, chatroom_read_loop, NULL);
      pthread_detach(chatroom_thread);

      printf("KeyCipher daemon running\n");
      while (running)
          sleep(1);

      server_stop();
      peer_manager_cleanup();

      printf("KeyCipher daemon stopped\n");
      return 0;
  }