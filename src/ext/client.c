#include "core/client.h"
#include "common.h"
#include "ntp.h"

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pulse/simple.h>
#include <pulse/error.h>

struct Client * client_p;

int stat=1;
int tot = 0, n = 0;

void sigint_handler (int signum) {
    if (client_p != NULL) {
        if (stat) {
            printf("%s exiting the chat\n", client_p->usr.name);
        }
        Client_exit (client_p);
        exit (0);
    }
}

void * read_handler (void * arg) {
    struct queue * q = client_p->response_q;

    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 2
    };

    pa_simple *s = NULL;
    int ret = 1, error;
    if (!(s = pa_simple_new(NULL, NULL, PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        goto finish;
    }
    while (1) {
        struct ServerResponse * resp = queue_pop (q);
        ssize_t r = 1024;
        switch (resp->type) {
            case MSG : {
                struct Msg * msg = resp->data;
                ll delay = timediff (msg->ts, resp->ts);
                time_t trecv = resp->ts.s;
                struct tm ts = *localtime (&trecv);
                char tstr[80];
                strftime (tstr, sizeof (tstr), "%H:%M:%S", &ts);
                printf ("[%s] <%s> : %s\n", tstr, msg->who, msg->msg);
                tot += delay;
                ++n;
                break;
            }
            case VMSG : {
                struct VMsg * msg = resp->data;
                if (pa_simple_write(s, msg->msg, (size_t) r, &error) < 0) {
                    fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
                    goto finish;
                }
                break;
            }
            case NOTIF : {
                struct Notification * notif = resp->data;
                printf ("[SERVER] %s\n", notif->msg);
                break;
            }
        }
        free (resp->data);
        free (resp);
    }
    finish:
        if (pa_simple_drain(s, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_drain() failed: %s\n", pa_strerror(error));
            goto finish;
        }
        if (s)
            pa_simple_free(s);
}

void * write_handler (void * arg) {
    char msg[256];
    while (1) {
        read (0, msg, 256);
        Client_send (client_p, msg, strlen (msg));
        memset (msg, '\0', 256);
    }
}

void * vwrite_handler (void * arg) {
    printf ("In voice_write_handler\n");
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 2
    };
    pa_simple *s = NULL;
    int error;
    if (!(s = pa_simple_new(NULL, NULL, PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__", %d: pa_simple_new() failed: %s\n", __LINE__, pa_strerror(error));
        goto finish;
    }
    int cnt = 0;
    for (;;) {
        struct VMsg msg;
        msg.grp = client_p->usr.room;
        ssize_t r;
        if (pa_simple_read(s, msg.msg, sizeof(msg.msg), &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            goto finish;
        }
        client_send_vmsg (client_p, &msg);
        cnt++;
    }
    finish:
        if (s)
            pa_simple_free (s);
    printf ("cnt=%d\n", cnt);
}

int DEBUG;

int main (int argc, char ** argv) {
    if (argc != 5 && argc != 6) {
        printf ("Usage: %s <serv_ip> <port> <name> <channel> <stat:opt>\n", argv[0]);
        exit (EXIT_FAILURE);
    }
    int voice = 0;
    if (argc == 6)
        voice = 1;

    client_p = malloc (sizeof (struct Client));
    Client_init (client_p, argv[1], atoi (argv[2]), argv[3], atoi (argv[4]));
    pthread_t tidvw,tidw,tidr;
    pthread_create (&tidw, NULL, write_handler, NULL);
    pthread_create (&tidr, NULL, read_handler, NULL);
    pthread_create (&tidvw, NULL, vwrite_handler, NULL);

    signal (SIGINT, sigint_handler);
    pthread_join (tidw, NULL);
    pthread_join (tidr, NULL);
    pthread_join (tidvw, NULL);

    Client_exit (client_p);
}
