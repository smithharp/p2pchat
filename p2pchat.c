#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "socket.h"
#include "ui.h"

#include "message.h"

// Keep the username in a global so we can access it from the callback
const char* username;

typedef struct args {
  int fd;
  unsigned short port; 
} args_t;

//Global variable for storing the connections
//This will have to be updated later to be an adjustable array or list
int connec_fd;
int connec_fd2;

//Linked list update to above thing ^^^^
typedef struct connection {
  int fd;
  struct connection * next;
} connection_t;

connection_t* connections = NULL;

// This function is run whenever the user hits enter after typing a message
void input_callback(const char* message) {
  //ADD CODE FOR SECTION
  if (strcmp(message, ":quit") == 0 || strcmp(message, ":q") == 0) {
    ui_exit();
  } else {
    //Display the message on host chat
    ui_display(username, message);
    //Add the username to the beginning of the message
    char usr_and_message[strlen(message) + strlen(username) + 1];
    usr_and_message[0] = '\0';
    strcat(usr_and_message, username);
    strcat(usr_and_message, ",");
    strcat(usr_and_message, message);
    //ui_display("usrmsg", usr_and_message);


    //Send the message to all the other chats
    connection_t* message_looper = connections;
    while(message_looper != NULL) {
      //Don't send a message to a closed connection (fd=-1)
      if(message_looper->fd != -1) {
        int fd = message_looper->fd;
        send_message(fd, (char*)usr_and_message);
      }
      message_looper = message_looper->next;
    }

  }
}

//This is the function we will use to actually read and relay messages
//thread for listening to existing connections for input 
void* thread_connection_fn(void* connection_args) {
  args_t* connection_arguments = (args_t*) connection_args;

  //This is to keep the chat open
  //The true condition here may need to be replaced with something to close connection
   while(true) {
    char * message = receive_message(connection_arguments->fd);
    //If receive_message() fails, loop through the list of connections
    //And set the fd of this connection to -1
    if(message == NULL) {
      connection_t* message_looper = connections;
      while(message_looper != NULL) {
        if(connection_arguments->fd == message_looper->fd) {
          message_looper->fd = -1;
        }
        message_looper = message_looper->next;
      }


      return NULL;
    }
    char display_message[strlen(message)];
    strcpy(display_message, message);
    //Parse out the username from the message
    //Since it will be sent as "usernamemessage"
    char* user = strtok(display_message,",");
    char* text = strtok(NULL, ",");
    ui_display(user, text);
    //ui_display("user?", message);
    //Send the message to all the other chats
    connection_t* message_looper = connections;
    while(message_looper != NULL) {
      //Don't send a message back to the same person or a closed connection (fd = -1)
      if(message_looper->fd != connection_arguments->fd && message_looper->fd != -1) {
        int fd = message_looper->fd;
        send_message(fd, (char*)message);
      }
      message_looper = message_looper->next;
    }
  }

  return NULL;
}

//thread for listening for new connections
void* thread_listening_fn(void* listening_args){ //????
  while(true) {
    args_t* listening_arguments = (args_t*) listening_args;
    //printf("Port: %u\n", listening_arguments->port);
    // Start listening for connections, with a maximum of one queued connection
    int server_socket_fd = listening_arguments->fd;
    // Start listening for connections, with a maximum of one queued connection
    if (listen(server_socket_fd, 1)) {
      perror("listen failed");
      exit(EXIT_FAILURE);
    }


    // Wait for a client to connect
    int client_socket_fd = server_socket_accept(server_socket_fd);
    if (client_socket_fd == -1) {
      perror("accept failed");
      exit(EXIT_FAILURE);
    }

    //printf("Client connected!\n");
    ui_display("System", "Client connected!");


    //Add this connection to our list

    //Initialize a new connection_t
    connection_t * new_connec = malloc(sizeof(connection_t));
    new_connec->fd = client_socket_fd;
    new_connec->next = NULL;

    //If we have no connections, make this the first
    if(connections == NULL) {
      connections = new_connec;
    }
    //Otherwise we have to find the end of the list
    else {
      //Create a placeholder pointer so we can find the end of the list
      connection_t* endOfList = connections;
      //Iterate to end of list
      while(endOfList->next != NULL) {
        endOfList = endOfList->next;
      }
      //Add it to the end of the list
      endOfList->next = new_connec;
    }
    //Initialize thread information and start thread
    pthread_t connection;
    args_t* connec_args = malloc(sizeof(args_t));
    connec_args->fd = client_socket_fd;
    pthread_create(&connection, NULL, thread_connection_fn, connec_args);

  }
  return 0;
}


int main(int argc, char** argv) {
  // Make sure the arguments include a username
  if (argc != 2 && argc != 4) {
    fprintf(stderr, "Usage: %s <username> [<peer> <port number>]\n", argv[0]);
    exit(1);
  }

  // Save the username in a global
  username = argv[1];

  // TODO: Set up a server socket to accept incoming connections
  unsigned short port = 0;
  int incoming_socket_fd = server_socket_open(&port);
  if (incoming_socket_fd == -1) {
    perror("Server socket was not opened");
    exit(EXIT_FAILURE);
  }


  pthread_t listener;
  args_t listener_args;
  listener_args.port = port;
  listener_args.fd = incoming_socket_fd;
  char port_string[10];
  sprintf(port_string, "%u", port);

  pthread_create(&listener, NULL, thread_listening_fn, &listener_args);

  // Did the user specify a peer we should connect to?
  if (argc == 4) {
    // Unpack arguments
    char* peer_hostname = argv[2];
    unsigned short peer_port = atoi(argv[3]);

    //Connect to the peer we have specified
    //Open the connection
    int client_socket = socket_connect(peer_hostname, peer_port);
    //Add their socket to our list of connections
    connections = malloc(sizeof(connection_t));
    connections->fd = client_socket;
    connections->next = NULL;
    //Open a thread for the connection
    pthread_t connec;
    args_t* connec_args = malloc(sizeof(args_t));
    connec_args->fd = client_socket;

    pthread_create(&connec, NULL, thread_connection_fn, connec_args);


    if (client_socket == -1){
      perror("Failed to create new client_socket and connect to new server.\n");
      exit(EXIT_FAILURE);
    } 
  }

  //Steps
  //Create an additional thread to do these things concurrently
  pthread_t thread;
  //Read input from keyboard or from another peer
  //And then relay it to all other connections (except the one we received it from)

  // Set up the user interface. The input_callback function will be called
  // each time the user hits enter to send a message.
  ui_init(input_callback);

  // Once the UI is running, you can use it to display log messages
  ui_display("Port", port_string);

  // Run the UI loop. This function only returns once we call ui_stop() somewhere in the program.
  ui_run();

  return 0;
}