#ifndef HOST_H
#define HOST_H
#include <stdint.h>
#include <arpa/inet.h>
#include <time.h>
#include "common/protocol_STUN.h"
#include "common/protocol_mess.h"
#include "tui/tui.h"
#define MAX_PEERS 8
#define TIMEOUT_PEER 60

struct peer {
    struct sockaddr_in addr;
    time_t timestamp; //do roznych rzeczy, do laczenia, do utrzymywania komunikacji
    char nick[NICK_LEN];
    int active;
};

typedef enum {
    HOST_STATE_START, //pyta o nazwe pokoju, wysyla register
    HOST_STATE_WAITING_REGISTERED, //czeka na registered
    HOST_STATE_HOSTING,
} host_start_state_t;

typedef enum {
    HOSTING_
} hosting_state_t;

void host_start(int sock, struct sockaddr_in *server, char* n, tui_t* tui);
void host_hosting(int sock, struct sockaddr_in *server, char* n, tui_t* tui);

/*zarowno wysyla punche do wszystkich z who jak i sprawdza czasy timeoutow i wyrzuca nieaktywnych*/
void send_punches(int sock, struct peer* who);
/*przyjmuje socket, liste peerow i ramke z wiadomoscia, typu CHAT_MSG ktora wysyla*/
void broadcast_mess(int sock, struct peer* who, uint8_t* msg);

//handlery
void handle_hosting_punch(int sock, struct sockaddr_in *sender, struct msg_header *hdr,
                          struct peer *pending, int *pending_count,
                          struct peer *connected, int *connected_count);
void handle_chat_join(int sock, struct sockaddr_in *sender, struct msg_header *hdr,
                      struct peer *connected, int *connected_count);
void handle_chat_msg(int sock, struct sockaddr_in *sender, struct msg_header *hdr,
                     struct peer *connected, int connected_count);
void handle_chat_leave(int sock, struct sockaddr_in *sender, struct msg_header *hdr,
                       struct peer *connected, int *connected_count);
void handle_chat_punch(int sock, struct sockaddr_in *sender, struct msg_header *hdr,
                     struct peer *connected, int connected_count);

#endif