#ifndef PEER_H
#define PEER_H
#include <stdint.h>
#include <arpa/inet.h>
#include <sodium.h>
#include "tui/tui.h"
#include "common/protocol_mess.h"

typedef enum {
    PEER_STATE_START, //wysyla MSG_LIST
    PEER_STATE_WAITING_LIST, //czeka na odpowiedz, odbiera i wyswietla
    PEER_STATE_BROWSING, //wyswietla liste i czeka na wybor pokoju przez uzytkownika, wysyla join
    PEER_STATE_CONNECTING,
    PEER_STATE_CONNECTED,
} peer_state_t;

void peer_start(int sock, struct sockaddr_in *server, char* nick, tui_t* tui, uint8_t* peer_pub, uint8_t* peer_sec);
void peer_chat(int sock, struct sockaddr_in *host, char* n, tui_t* tui, uint8_t* peer_pub, uint8_t* peer_sec);
#endif