#define DECL_QUEUE_SR

#include "core/client.h"
#include "common.h"
#include "ntp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pulse/simple.h>
#include <pulse/error.h>

extern int DEBUG;

#define CALL(n, msg) if ((n) < 0) {fprintf (stderr, "%s (%s:%d)\n", msg, __FILE__, __LINE__); exit (EXIT_FAILURE);}
#define LOG(...) do {if (DEBUG) {fprintf (stderr, __VA_ARGS__); fprintf (stderr, "\n");}} while (0);

void * listen_handler (void * arg);

void Client_init (struct Client * client, char * serv_ip, int port, char * name, int room) {
    client->response_q = queue_new ();
    // initalise the mutex with default attributes
    pthread_mutex_init (&client->lock, NULL);

    pthread_mutex_lock (&client->lock); //lock the mutex
    CALL (client->sock_fd = socket (AF_INET, SOCK_STREAM, 0), "socket"); //create IPV4 socket
    //set all the fiels of serveraddr_in to 0 (not necessary)
    memset(&client->serveraddr_in, '0', sizeof (struct sockaddr_in));
    client->serveraddr_in.sin_family = AF_INET;
    client->serveraddr_in.sin_addr.s_addr = inet_addr (serv_ip); //server ip to byte
    client->serveraddr_in.sin_port = htons(port); //host byte to network byte

    CALL (connect ( client->sock_fd,
                    (struct sockaddr *)&client->serveraddr_in,
                    sizeof(struct sockaddr_in)),

        "connect");

    LOG ("Connected to Server");
    printf("Connected to Server\n");
    ntp_init ();

    // strcpy (client->usr.name, name);
    int req = JOIN_ROOM; //enum
    CALL (write (client->sock_fd, &req, sizeof (int)), "write");

    //set client name as name given by user. see common.h
    struct User usr; strcpy (usr.name, name); usr.room = room;
    client->usr = usr;
    //send room/group and name to server
    CALL (write (client->sock_fd, &room, sizeof (int)), "write");
    CALL (write (client->sock_fd, &usr, sizeof (struct User)), "write");

    pthread_mutex_unlock (&client->lock);
    //create a thread for listen_handler with default attributes
    pthread_create (&client->tidr, NULL, listen_handler, (client));
}

void Client_send (struct Client * client, char * buf, size_t len) {
    struct Msg msg;
    static int req = MSG;
    msg.grp = client->usr.room;
    msg.ts = gettime ();

    //create the message to be sent
    strcpy (msg.who, client->usr.name);
    strcpy (msg.msg, buf);

    pthread_mutex_lock (&client->lock);
    CALL (write (client->sock_fd, &req, sizeof (int)), "write");
    CALL (write (client->sock_fd, &msg, sizeof (struct Msg)), "write");
    pthread_mutex_unlock (&client->lock);
}

void client_send_vmsg (struct Client * client, struct VMsg * vmsg) {
    static int req = VMSG;
    write (client->sock_fd, &req, sizeof (int));
    write (client->sock_fd, vmsg, sizeof (struct VMsg));
}

void * listen_handler (void * arg) {
    struct Client * client = (struct Client *)arg;
    struct queue * q = client->response_q;
    int req;
    while (read (client->sock_fd, &req, sizeof (int))) {
        struct t_format ts = gettime ();
        switch (req) {
            case MSG : {
                struct Msg * msg = malloc (sizeof (struct Msg));
                int b = read (client->sock_fd, msg, sizeof (struct Msg));
                if (b < 0) return NULL;
                struct ServerResponse * resp = malloc (sizeof (struct ServerResponse));
                resp->type = req;
                resp->data = msg;
                resp->ts = ts;
                queue_push (q, resp);
                break;
            }
            case VMSG : {
              //  printf("Entering the voice message recieve\n");
                struct VMsg * msg = malloc (sizeof (struct VMsg));
                int b = read (client->sock_fd, msg, sizeof (struct VMsg));
                if (b < 0) {
                    return NULL;
                }
                struct ServerResponse * resp = malloc (sizeof (struct ServerResponse));
                resp->type = req;
                resp->data = msg;
                resp->ts = ts;
                queue_push (q, resp);
                break;
            }
            case NOTIF : {
                struct Notification * notif = malloc (sizeof (struct Notification));
                int b = read (client->sock_fd, notif, sizeof (struct Notification));
                if (b < 0) return NULL;
                struct ServerResponse * resp = malloc (sizeof (struct ServerResponse));
                resp->type = req;
                resp->data = notif;
                queue_push (q, resp);
                break;
            }
            case BYE : {
                LOG ("Server quit.");
                printf("Server Quit\n");
                return NULL;
            }
        }
    }
}

void Client_exit (struct Client * client) {
    int req = LEAVE_ROOM;
    pthread_mutex_lock (&client->lock);
    int w = write (client->sock_fd, &req, sizeof (req));
    if (w < 0) {}
    close (client->sock_fd);
    LOG ("Client Left");
}
