#ifndef PROTOCOL_MESS_H
#define PROTOCOL_MESS_H
#define NICK_LEN 16
#define MESS_LEN 256
#include <sodium.h>

typedef enum {
    CHAT_PUNCH      = 0x01, // hole punching w celu podtrzymania
    CHAT_JOIN       = 0x02, // wysyla nick
    CHAT_JOIN_OK    = 0x03, // host potwierdza ze zjoinowal
    CHAT_MSG        = 0x04, // wiadomość tekstowa
    CHAT_KICK       = 0x05, // host kickuje peera
    CHAT_LEAVE      = 0x06, // peer wychodzi
    CHAT_CLOSE_ROOM = 0x07, // host zamyka pokoj
} chat_msg_type_t;

//nagłówek taki jak w protocol_stun.h

// --- payloady ---
struct chat_payload_join {
    char name[NICK_LEN];
    uint8_t public_key[crypto_kx_PUBLICKEYBYTES];
} __attribute__((packed));

struct chat_payload_join_ok {
    uint8_t public_key[crypto_kx_PUBLICKEYBYTES];
} __attribute__((packed));

struct chat_payload_msg {
    char name[NICK_LEN];
    char mess[MESS_LEN];
} __attribute__((packed));

struct chat_payload_kick {
    char reason[MESS_LEN];
} __attribute__((packed));

struct chat_payload_leave {
    char name[NICK_LEN];
} __attribute__((packed));



#endif