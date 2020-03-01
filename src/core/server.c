#include "core/server.h"
#include "ds/list.h"
#include "ds/queue.h"
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

extern int DEBUG;
#define LOG(...) do {if (DEBUG) {fprintf (stderr, __VA_ARGS__); fprintf (stderr, "\n");}} while (0);
#define CALL(n, msg) if ((n) < 0) {fprintf (stderr, "%s (%s:%d)\n", msg, __FILE__, __LINE__); exit (EXIT_FAILURE);}

void * connection_handler (void *);
void * broadcast_handler (void *);
void * notification_handler (void *);
void dict_init ();
void queue_init ();

void Server_init (struct Server * server, int port) {
    CALL (server->sock_fd = socket (AF_INET, SOCK_STREAM, 0), "socket")  //create IPV4 socket
    //set all the fields of serveraddr_in to 0 (not necessary)
    memset (&server->serveraddr_in, '0', sizeof (struct sockaddr_in));
    server->serveraddr_in.sin_family = AF_INET;
    server->serveraddr_in.sin_addr.s_addr = htonl (INADDR_ANY); //host byte to network byte
    server->serveraddr_in.sin_port = htons(port); //host byte to network byte

    int true = 1;
    //set option to reuse local addresses
    CALL (setsockopt(server->sock_fd, SOL_SOCKET, SO_REUSEADDR, &true, sizeof (int)), "setsockopt");
    CALL (bind(server->sock_fd, (struct sockaddr*)&server->serveraddr_in, sizeof(struct sockaddr_in)), "bind");
    CALL (listen(server->sock_fd, 100), "listen"); //listen for total 100 connections

    signal (SIGPIPE, SIG_IGN); //ignore the broken pipe signal
    dict_init (); //initalise lists for each group
    queue_init (); //initalise the message queue
}

void Server_listen (struct Server * server) {
    printf("Server initalised\n");
    LOG ("Server running.")
    pthread_t tidb, tidn;
    pthread_create (&tidb, NULL, broadcast_handler, NULL);
    pthread_create (&tidn, NULL, notification_handler, NULL);
    while (1) {
        int *connfd = malloc (sizeof (int));
        //accept new connection
        CALL (*connfd = accept (server->sock_fd, (struct sockaddr*)NULL ,NULL), "accept");
        LOG ("New Connection.");
        pthread_t tid;
        //once a new client joins, initalise the connection_handler
        CALL (pthread_create (&tid, NULL, connection_handler, connfd), "pthread");
        usleep (20*1000);
    }
}

#define MAX_GROUPS 100

//declaring the list interface. See ds/list.h
DECL_LIST_INTERFACE(int);
DECL_LIST_IMPL(int);
struct list dict[MAX_GROUPS];

void close_connection (int fd) {
    int req = BYE;
    write (fd, &req, sizeof (int));
    LOG ("%d ", fd);
    close (fd);
}

void Server_exit (struct Server * server) {
    for (int i = 0; i < MAX_GROUPS; ++i) {
        //calls the close connection function for each and frees the dict
        list_foreach (&dict[i], close_connection);
        list_free (&dict[i]);
    }
    printf("Server exited\n");
    LOG ("Server exited");
}

struct ClientResponse {
    struct Msg * msg;
    int connfd;
};
struct ClientNotification {
    struct Notification notif;
    int grp;
    int connfd;
};
struct queue * msgq;
struct queue * notifq;

static void notifq_push (struct ClientNotification * notif) {
    queue_push (notifq, notif);
}
static void msgq_push (struct ClientResponse * resp) {
    queue_push (msgq, resp);
}

void queue_init () {
    msgq = queue_new ();
    notifq = queue_new ();
}

void dict_init () {
    for (size_t i = 0; i < MAX_GROUPS; ++i) {
        list_init (&dict[i]);
    }
}
void dict_insert (int key, int val) {
    list_insert (&dict[key], val);
}
void dict_remove (int key, int val) {
    list_remove (&dict[key], val);
}

//useful macros
#define READ_REQ() read (connfd, &req, sizeof (int))
#define READ(what, size) read (connfd, what, size)

void * connection_handler (void * arg) {
    LOG ("In %s\n", __func__);
    int connfd = *(int*)arg;
    int req, room;
    struct User usr;
    // setup the first Request i.e join a room
    {
        CALL (READ_REQ (), "read");
        switch (req) {
            case JOIN_ROOM : {
                READ(&room, sizeof (int));
                READ(&usr, sizeof (struct User));
                dict_insert (room, connfd);
                LOG ("New user [%s] in room[%d]\n", usr.name, room);
                printf("New user %s in room %d\n", usr.name, room);
                struct ClientNotification * notif = malloc (sizeof (struct ClientNotification));
                // READ (&notif->notif.ts, sizeof (struct t_format));
                notif->grp = room;
                notif->connfd = connfd;
                sprintf (notif->notif.msg, "%s has joined your room", usr.name);
                notifq_push (notif);
                break;
            }

        }
    }

    // to look for incoming Requests
    {
        while (READ_REQ()) {
            // READ_REQ ();
            switch (req) {
                case LEAVE_ROOM : {
                    dict_remove (room, connfd);
                    struct ClientNotification * notif = malloc (sizeof (struct ClientNotification));
                    // READ (&notif->notif.ts, sizeof (struct t_format));
                    notif->grp = room;
                    notif->connfd = connfd;
                    printf ("%s has left the room %d\n", usr.name, room);
                    sprintf (notif->notif.msg, "%s has left your room", usr.name);
                    notifq_push (notif);
                    dict_remove (room, connfd);
                    close (connfd);
                    free (arg);
                    LOG ("closed connection %d", connfd);
                    return NULL;
                }
                case MSG : {
                    struct Msg * msg = malloc (sizeof (struct Msg));
                    CALL (READ (msg, sizeof (struct Msg)), "read");
                    struct ClientResponse * resp = malloc (sizeof (struct ClientResponse));
                    resp->msg = msg;
                    resp->connfd = connfd;
                    msgq_push (resp);
                    break;
                }
            }
        }
    }
}

// will send all the messages stord in message queue
void * broadcast_handler (void * arg) {
    while (1) {
        struct ClientResponse * resp = queue_pop (msgq);
        struct Msg * msg = resp->msg;
        int grp = msg->grp, connfd = resp->connfd;

        pthread_mutex_lock (&dict[grp].lock);
        int serv_op = MSG, tot = 0;
        struct node * curr = dict[grp].head;
        while (curr) {
            if (curr->val == connfd) {
                curr = curr->nxt;
                continue;
            }
            ++tot;
            int w1 = write (curr->val, &serv_op, sizeof (int));
            int w2 = write (curr->val, msg, sizeof (struct Msg));
            curr=curr->nxt;
        }
        free (msg);
        pthread_mutex_unlock (&dict[grp].lock);
    }
}

void * notification_handler (void * arg) {
    while (1) {
        struct ClientNotification * notif = queue_pop (notifq);
        int grp = notif->grp, connfd = notif->connfd;

        pthread_mutex_lock (&dict[grp].lock);
        int serv_op = NOTIF, tot = 0;
        struct node * curr = dict[grp].head;
        while (curr) {
            if (curr->val == connfd) {
                curr = curr->nxt;
                continue;
            }
            ++tot;
            write (curr->val, &serv_op, sizeof (int));
            write (curr->val, &notif->notif, sizeof (struct Notification));
            curr=curr->nxt;
        }
        free (notif);
        pthread_mutex_unlock (&dict[grp].lock);
    }
}

#undef READ_REQ
#undef READ
