#pragma once

enum Request {
    JOIN_ROOM,
    SEND_MSG,
    LEAVE_ROOM,
};

enum ServerOps {
    MSG,
    VMSG,
    BYE,
    NOTIF,
};

typedef long long ll;

struct t_format {
    ll s;
    ll us;
};

struct User {
    char name[20];
    int room;
};

struct Notification {
    char msg[256];
    struct t_format ts;
};
struct Msg {
    int grp;
    struct t_format ts;
    char who[20];
    char msg[256];
};
struct VMsg {
    int grp;
    uint8_t msg[1024];
};
