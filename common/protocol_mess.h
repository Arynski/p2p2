#ifndef PROTOCOL_MESS_H
#define PROTOCOL_MESS_H
#define NICK_LEN 16
#define MESS_LEN 256

typedef enum {
    CHAT_PUNCH      = 0x01, // hole punching w celu podtrzymania
    CHAT_JOIN       = 0x02, // wysyla nick
    //CHAT_JOIN_OK    = 0x03, // host potwierdza ze zjoinowal
    CHAT_MSG        = 0x04, // wiadomość tekstowa
    CHAT_KICK       = 0x05, // host kickuje peera
    CHAT_LEAVE      = 0x06, // peer wychodzi
} chat_msg_type_t;

//nagłówek taki jak w protocol_stun.h

// --- payloady ---
struct chat_payload_join {
    char name[NICK_LEN];
} __attribute__((packed));

struct chat_payload_msg {
    char mess[MESS_LEN];
} __attribute__((packed));

struct chat_payload_kick {
    char reason[MESS_LEN];
} __attribute__((packed));


#endif