#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <syslog.h>
#include "common/network.h"
#include "common/protocol_STUN.h"
#include "room.h"

static int payload_too_small(struct msg_header *hdr, size_t expected) {
    return ntohs(hdr->payload_len) < expected;
}

size_t build_frame(uint8_t *buf, uint8_t type, const void *payload, uint16_t payload_len) {
    struct msg_header *hdr = (struct msg_header *)buf;
    hdr->type = type;
    hdr->payload_len = htons(payload_len);
    if (payload && payload_len > 0)
        memcpy(buf + sizeof(struct msg_header), payload, payload_len);
    return sizeof(struct msg_header) + payload_len;
}

void handle_payload(int sock, struct sockaddr_in *sender, struct msg_header *hdr) {
    //fprintf(stdout, "\n[SERWER] Odebrano pakiet typu: %d od %s:%d\n", 
    //        hdr->type, inet_ntoa(sender->sin_addr), ntohs(sender->sin_port));
    fflush(stdout);
    switch (hdr->type) {
            case MSG_REGISTER:   handle_register(sock, sender, hdr); break;
            case MSG_UNREGISTER: handle_unregister(sock, sender, hdr); break;
            case MSG_PING:       handle_ping(sock, sender, hdr); break;
            case MSG_LIST:       handle_list(sock, sender, hdr); break;
            case MSG_JOIN:       handle_join(sock, sender, hdr); break;
            default:
                syslog(LOG_WARNING, "Nieznany typ pakietu: %d", hdr->type);
                break;
    }
}    

void handle_register(int sock, struct sockaddr_in *sender, struct msg_header *hdr) {
    if (payload_too_small(hdr, sizeof(struct payload_register)))
        return;

    struct payload_register *data = (struct payload_register *)hdr->payload;
    struct sockaddr_in loc_addr = {0};
    loc_addr.sin_family = AF_INET;
    loc_addr.sin_addr.s_addr = data->host_local_ip;
    loc_addr.sin_port = data->host_local_port;
    int slot = room_add(data->name, sender, &loc_addr); 
    uint8_t resp[BUF_SIZE]; //odsylana ramka
    size_t len; 
    if(slot == -1) {
        //odsyla MSG_ERROR
        struct payload_error err = {0};        
        strncpy(err.message, "Nie udalo sie stworzyc pokoju! Istnieje juz maksymalna ilosc.", sizeof(err.message) - 1);
        err.message[sizeof(err.message)-1] = '\0';

        len = build_frame(resp, MSG_ERROR, &err, sizeof(err));
    } else {
        //odsyla MSG_REGISTERED
        struct payload_registered reged;
        reged.room_id = htonl(rooms[slot].id); //zeby na pewno bylo w network_byte_order

        len = build_frame(resp, MSG_REGISTERED, &reged, sizeof(reged));
    }
    net_send(sock, resp, len, sender);
}   

/*Obsługuje unregister, dezaktywuje pokój*/
void handle_unregister(int sock, struct sockaddr_in *sender, struct msg_header *hdr) {
    if (payload_too_small(hdr, sizeof(struct payload_unregister)))
        return;
    struct payload_unregister *data = (struct payload_unregister *)hdr->payload;
    room_remove(ntohl(data->room_id));
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
    uint16_t count = room_get_active(resp_payload->rooms, MAX_ROOMS);
    resp_payload->count = count;
    
    //na network byte order
    for(int i = 0; i < count; i++) {
        resp_payload->rooms[i].room_id = htonl(resp_payload->rooms[i].room_id);
    }

    size_t len = build_frame(resp, MSG_LIST_RESP, resp_payload,
        sizeof(struct payload_list_resp) + sizeof(struct room_entry) * count);
    
    net_send(sock, resp, len, sender);
}

/*Synchronizuje peera z hostem*/
void handle_join(int sock, struct sockaddr_in *sender, struct msg_header *hdr) {
    if (payload_too_small(hdr, sizeof(struct payload_join)))
        return;
    struct payload_join *data = (struct payload_join *)hdr->payload;
    syslog(LOG_DEBUG, "JOIN do pokoju %u od %s:%d", 
       ntohl(data->room_id), 
       inet_ntoa(sender->sin_addr), 
       ntohs(sender->sin_port));
    struct room* jointo = room_find(ntohl(data->room_id));
    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = data->peer_local_ip;
    local_addr.sin_port = data->peer_local_port;
    memset(local_addr.sin_zero, 0, sizeof(local_addr.sin_zero));

    if(jointo != NULL) {
        //wysylane do hosta vvv (adresy peera)
        uint8_t respHost[sizeof(struct msg_header) + sizeof(struct payload_punch)];
        struct payload_punch hostPayload; 
        hostPayload.public_addr = *sender;
        hostPayload.local_addr = local_addr;
        size_t lenHost = build_frame(respHost, MSG_PUNCH, &hostPayload, sizeof(hostPayload));

        //wysylane do peera vvv
        uint8_t respPeer[sizeof(struct msg_header) + sizeof(struct payload_punch)];
        struct payload_punch peerPayload; 
        peerPayload.public_addr = jointo->public_host_addr;
        peerPayload.local_addr = jointo->local_host_addr;
        size_t lenPeer = build_frame(respPeer, MSG_PUNCH, &peerPayload, sizeof(peerPayload));

        net_send(sock, respHost, lenHost, &jointo->public_host_addr);
        net_send(sock, respPeer, lenPeer, sender);
    } else {
        //przesylamy blad, ze proba polaczenia z nieistniejacym pokojem
        uint8_t resp[sizeof(struct msg_header) + sizeof(struct payload_error)];
        struct payload_error err = {0};
        strncpy(err.message, "Blad! Proba polaczenia z nieistniejacym pokojem!", sizeof(err.message) - 1);
        err.message[sizeof(err.message) - 1] = '\0';
        int len = build_frame(resp, MSG_ERROR, &err, sizeof(err));
        
        net_send(sock, resp, len, sender);
    }
}