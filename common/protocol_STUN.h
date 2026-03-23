#ifndef PROTOCOL_STUN_H
#define PROTOCOL_STUN_H

#include <stdint.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024
#define MAX_ROOMS 16

/*
w zasadzie musi: 
    - przechowywac pokoje i zarządzać (tworzyc, moze usuwac itd)
    - odpowiadać listą pokojów
    - utrzymywac dziurę UDP z hostem (ale to host wysyla PING), 
        z peerami nie trzeba, bo tam komunikacja jest tylko peer -> serwer 
        i odpowiedź, jak normalnie. Host musi, bo to serwer go informuje (nie 
        wiadomo kiedy, ze ktos chce dolaczyc)
    - synchronizowac UDP punche
*/

//rodzaje wiadomosci, 2 bajty
typedef enum {
    MSG_REGISTER        = 0x01, //host -> serwer
    MSG_REGISTERED      = 0x02, //serwer -> host, ACK
    MSG_UNREGISTER      = 0x03, //host -> serwer
    MSG_PING            = 0x04, //host -> serwer
    MSG_LIST            = 0x05, //peer -> serwer
    MSG_LIST_RESP       = 0x06, //serwer -> peer
    MSG_JOIN            = 0x07, //peer -> serwer
    MSG_PUNCH           = 0x08, //serwer -> peer i serwer -> host
    MSG_ERROR           = 0x09, //jak bedzie blad no 
} msg_type_t;

//ramka, nagłówek i różne payloady

//moze isc w przyszlosci do wspolnego protocol.h
struct msg_header {
    uint8_t  type;
    uint16_t payload_len;
    uint8_t  payload[];
} __attribute__((packed));

// --- payloady ---
struct payload_register {
    char name[64];
} __attribute__((packed));

struct payload_registered {
    uint32_t room_id;
} __attribute__((packed));

struct payload_unregister {
    uint32_t room_id;
} __attribute__((packed));

struct room_entry {
    uint32_t room_id;
    char     name[64];
} __attribute__((packed));

struct payload_list_resp {
    uint8_t           count;
    struct room_entry rooms[];
} __attribute__((packed));

struct payload_join {
    uint32_t room_id;
} __attribute__((packed));

struct payload_punch {
    struct sockaddr_in addr;
} __attribute__((packed));

struct payload_error {
    char message[64];
} __attribute__((packed));


#endif