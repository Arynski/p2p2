#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <syslog.h>
#include "common/network.h"
#include "common/protocol_STUN.h"
#include "room.h"

size_t build_frame(uint8_t *buf, uint8_t type, const void *payload, uint16_t payload_len) {
    struct msg_header *hdr = (struct msg_header *)buf;
    hdr->type = type;
    hdr->payload_len = payload_len;
    if (payload && payload_len > 0)
        memcpy(buf + sizeof(struct msg_header), payload, payload_len);
    return sizeof(struct msg_header) + payload_len;
}

void handle_payload(int sock, struct sockaddr_in *sender, struct msg_header *hdr) {
    syslog(LOG_DEBUG, "Handle payload, typ: %d", hdr->type);
    switch (hdr->type) {
            case MSG_REGISTER:   handle_register(sock, sender, hdr); break;
            case MSG_UNREGISTER: handle_unregister(sock, sender, hdr); break;
            case MSG_PING:       handle_ping(sock, sender, hdr); break;
            case MSG_LIST:       handle_list(sock, sender, hdr); break;
            case MSG_JOIN:       handle_join(sock, sender, hdr); break;
            default: break;
    }
}    

void handle_register(int sock, struct sockaddr_in *sender, struct msg_header *hdr) {
    struct payload_register *data = (struct payload_register *)hdr->payload;
    int slot = room_add(data->name, sender);
    uint8_t resp[BUF_SIZE]; //odsylana ramka
    size_t len; 
    if(slot == -1) {
        //odsyla MSG_ERROR
        struct payload_error err;        
        strncpy(err.message, "Nie udalo sie stworzyc pokoju! Istnieje juz maksymalna ilosc.", sizeof(err.message) - 1);
        
        len = build_frame(resp, MSG_ERROR, &err, sizeof(err));
    } else {
        //odsyla MSG_REGISTERED
        struct payload_registered reged;
        reged.room_id = rooms[slot].id;

        len = build_frame(resp, MSG_REGISTERED, &reged, sizeof(reged));
    }
    net_send(sock, resp, len, sender);
}   

/*Obsługuje unregister, dezaktywuje pokój*/
void handle_unregister(int sock, struct sockaddr_in *sender, struct msg_header *hdr) {
    struct payload_unregister *data = (struct payload_unregister *)hdr->payload;
    room_remove(data->room_id);
}

/*Obsługuje ping z serwera*/
void handle_ping(int sock, struct sockaddr_in *sender, struct msg_header *hdr) {
    int index = room_find_by_host(sender);
    if(index == -1) return;
    rooms[index].last_ping = time(NULL);
}

/*Obsługuje żadanie listy pokojów*/
void handle_list(int sock, struct sockaddr_in *sender, struct msg_header *hdr) {
    uint8_t resp[sizeof(struct msg_header) + sizeof(struct payload_list_resp) + sizeof(struct room_entry) * MAX_ROOMS];
    
    struct payload_list_resp *resp_payload = (struct payload_list_resp *)(resp + sizeof(struct msg_header));
    resp_payload->count = room_get_active(resp_payload->rooms, MAX_ROOMS);
    
    size_t len = build_frame(resp, MSG_LIST_RESP, resp_payload,
        sizeof(struct payload_list_resp) + sizeof(struct room_entry) * resp_payload->count);
    
    net_send(sock, resp, len, sender);
}

/*Synchronizuje peera z hostem*/
void handle_join(int sock, struct sockaddr_in *sender, struct msg_header *hdr) {
    syslog(LOG_DEBUG, "Otrzymano join request od %d, %d", sender->sin_port, sender->sin_addr);
    struct payload_join *data = (struct payload_join *)hdr->payload;
    struct room* jointo = room_find(data->room_id);
    
    if(jointo != NULL) {
        uint8_t respHost[sizeof(struct msg_header) + sizeof(struct payload_punch)];
        struct payload_punch hostPayload; hostPayload.addr = *sender;
        size_t lenHost = build_frame(respHost, MSG_PUNCH, &hostPayload, sizeof(hostPayload));

        uint8_t respPeer[sizeof(struct msg_header) + sizeof(struct payload_punch)];
        struct payload_punch peerPayload; peerPayload.addr = jointo->host_addr;
        size_t lenPeer = build_frame(respPeer, MSG_PUNCH, &peerPayload, sizeof(peerPayload));

        net_send(sock, respHost, lenHost, &jointo->host_addr);
        net_send(sock, respPeer, lenPeer, sender);
    } else {
        //przesylamy blad, ze proba polaczenia z nieistniejacym pokojem
        uint8_t resp[sizeof(struct msg_header) + sizeof(struct payload_error)];
        struct payload_error err;
        strncpy(err.message, "Blad! Proba polaczenia z nieistniejacym pokojem!", sizeof(err.message) - 1);

        int len = build_frame(resp, MSG_ERROR, &err, sizeof(err));
        
        net_send(sock, resp, len, sender);
    }
}