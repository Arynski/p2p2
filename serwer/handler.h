#ifndef HANDLER_H
#define HANDLER_H
#include <stdint.h>
#include <stddef.h>
#include <arpa/inet.h>
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

//obsługują przychodzące wiadomości

/*Obsługuje wiadomosc typu register, tworzy nowy pokój (jak może)*/
void handle_register(int sock, struct sockaddr_in *sender, struct msg_header *hdr);

/*Obsługuje unregister, dezaktywuje pokój*/
void handle_unregister(int sock, struct sockaddr_in *sender, struct msg_header *hdr);

/*Obsługuje ping z serwera*/
void handle_ping(int sock, struct sockaddr_in *sender, struct msg_header *hdr);

/*Obsługuje żadanie listy pokojów*/
void handle_list(int sock, struct sockaddr_in *sender, struct msg_header *hdr);

/*Synchronizuje peera z hostem*/
void handle_join(int sock, struct sockaddr_in *sender, struct msg_header *hdr);

/*Sprawdza jaki jest rodzaj i wybiera odpowiednia funkcje z handle*/
void handle_payload(int sock, struct sockaddr_in *sender, struct msg_header *hdr);


#endif