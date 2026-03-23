#ifndef HANDLER_H
#define HANDLER_H
#include <stdint.h>
#include <stddef.h>
#include <arpa/inet.h>
#include "common/protocol_mess.h"
#include "common/protocol_STUN.h"
/*    
    MSG_REGISTER        = 0x01, //host -> serwer
    MSG_REGISTERED      = 0x02, //serwer -> host, ACK
    MSG_UNREGISTER      = 0x03, //host -> serwer
    MSG_PING            = 0x04, //host -> serwer
    MSG_LIST            = 0x05, //peer -> serwer
    MSG_LIST_RESP       = 0x06, //serwer -> peer
    MSG_JOIN            = 0x07, //peer -> serwer
    MSG_PUNCH           = 0x08, //serwer -> peer i serwer -> host
    MSG_ERROR           = 0x09, //jak bedzie blad no 
*/

/*Pomocnicza funkcja ktora buduje ramke*/
size_t build_frame(uint8_t *buf, uint8_t type, const void *payload, uint16_t payload_len);

#endif