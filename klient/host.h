#ifndef HOST_H
#define HOST_H
#include <stdint.h>
#include <arpa/inet.h>
#include <time.h>
#include <sodium.h>
#include "common/protocol_STUN.h"
#include "common/protocol_mess.h"
#include "tui/tui.h"
#define MAX_PEERS 8
#define TIMEOUT_PEER 60

struct peer {
    struct sockaddr_in used_addr; //albo public albo local
    struct sockaddr_in public_addr;
    struct sockaddr_in local_addr;
    time_t timestamp; //do roznych rzeczy, do laczenia, do utrzymywania komunikacji
    char nick[NICK_LEN];
    int active;
    uint8_t session_key_tx[crypto_kx_SESSIONKEYBYTES];
    uint8_t session_key_rx[crypto_kx_SESSIONKEYBYTES];
};

typedef enum {
    HOST_STATE_START, //pyta o nazwe pokoju, wysyla register
    HOST_STATE_WAITING_REGISTERED, //czeka na registered
    HOST_STATE_HOSTING,
} host_start_state_t;

typedef enum {
    HOSTING_
} hosting_state_t;

void host_start(int sock, struct sockaddr_in *server, char* n, tui_t* tui, uint8_t* host_pub, uint8_t* host_sec);
void host_hosting(int sock, struct sockaddr_in *host, char* n, tui_t* tui, uint8_t* host_pub, uint8_t* host_sec, uint32_t r_idx);

/*zarowno wysyla punche do wszystkich z who jak i sprawdza czasy timeoutow i wyrzuca nieaktywnych*/
void send_punches(int sock, struct peer* who, tui_t* tui);
/*przyjmuje socket, liste peerow i ramke z wiadomoscia, typu JAKIEGOS ktora wysyla*/
void broadcast_pack(int sock, struct peer* who, uint8_t* msg, struct sockaddr_in *sender, tui_t* tui);
/*przyjmuje socket, liste peerow i ramke z wiadomoscia, typu MSG_CHAT ktora wysyla*/
void broadcast_mess(int sock, struct peer* who, const char* sender_name, const char* clean_text, struct sockaddr_in *sender, tui_t* tui);
/*jak host chce wyjsc to do wszystkich wysylamy ze no wyszedl nara*/
void close_room(int sock, struct peer* who, tui_t* tui);

//handlery
void handle_hosting_punch(int sock, struct sockaddr_in *sender, struct msg_header *hdr,
                          struct peer *pending, int *pending_count,
                          struct peer *connected, int *connected_count,
                          tui_t* tui);
void handle_chat_join(int sock, struct sockaddr_in *sender, struct msg_header *hdr,
                      struct peer *connected, int *connected_count, tui_t* tui,
                      uint8_t* key_host_sec, uint8_t* key_host_pub);
void handle_chat_msg(int sock, struct sockaddr_in *sender, struct msg_header *hdr,
                     struct peer *connected, int *connected_count, tui_t* tui);
void handle_chat_leave(int sock, struct sockaddr_in *sender, struct msg_header *hdr,
                       struct peer *connected, int *connected_count, tui_t* tui);
void handle_chat_punch(int sock, struct sockaddr_in *sender, struct msg_header *hdr,
                     struct peer *connected, int connected_count);

bool get_sender_idx(int* idx, struct sockaddr_in *sender, struct peer *connected);

#endif