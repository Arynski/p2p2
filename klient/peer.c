#include <stdio.h>
#include <string.h>
#include <time.h>
#include "peer.h"
#include "common/network.h"
#include "handler.h"
#include "common/protocol_mess.h"
#include "tui/tui.h"
#include <sys/select.h>

void peer_start(int sock, struct sockaddr_in *server, char* n, tui_t* tui) {
    uint8_t buf[BUF_SIZE];
    peer_state_t stan = PEER_STATE_START;
    uint8_t lista_buf[sizeof(struct payload_list_resp) + sizeof(struct room_entry) * MAX_ROOMS];
    struct payload_list_resp *pokoje = (struct payload_list_resp *)lista_buf;
    struct sockaddr_in host_addr_public; host_addr_public.sin_addr.s_addr = 0;
    struct sockaddr_in host_addr_local; host_addr_local.sin_addr.s_addr = 0;
    struct sockaddr_in host_addr_used; host_addr_used.sin_addr.s_addr = 0;
    while(1) {
        fflush(stdout);
        switch(stan) {
            case PEER_STATE_START: {
                size_t len = build_frame(buf, MSG_LIST, NULL, 0);
                net_send(sock, buf, len, server);
                tui_log(tui, "Wyslano prosbe o liste!");
                stan = PEER_STATE_WAITING_LIST;
                break;
            }       
            case PEER_STATE_WAITING_LIST: {
                int n = net_recv(sock, buf, BUF_SIZE, NULL);
                if(n < 0) break;
                if((size_t)n < sizeof(struct msg_header)) break; //śmieci
                struct msg_header* hdr = (struct msg_header*)buf;
                if(hdr->type == MSG_LIST_RESP) {
                    memcpy(lista_buf, hdr->payload, ntohs(hdr->payload_len));
                    tui_get_list(tui, pokoje);
                    tui_log(tui, "Otrzymano odpowiedz na prosbe o liste!");
                    stan = PEER_STATE_BROWSING;
                }
                else if(hdr->type == MSG_ERROR) {
                    printf("Błąd!\n");
                }
                break;
            }
            case PEER_STATE_BROWSING: {
                if(tui_process_input(tui)) {
                    struct payload_join data;
                    struct sockaddr_in loc_addr = net_get_local_sockaddr(sock);
                    data.room_id = pokoje->rooms[tui->option].room_id;
                    data.peer_local_ip = loc_addr.sin_addr.s_addr;
                    data.peer_local_port = loc_addr.sin_port;
                    size_t len = build_frame(buf, MSG_JOIN, &data, sizeof(data));
                    net_send(sock, buf, len, server);

                    net_set_timeout(sock, 3, 0); //timeout do laczenia sie
                    stan = PEER_STATE_CONNECTING;
                }
                break;
            }
            case PEER_STATE_CONNECTING: {
                struct sockaddr_in sender;
                int n = net_recv(sock, buf, BUF_SIZE, &sender); //maks 3 sekundy bedzie
                fflush(stdout);
                if(n < 0) break;
                if((size_t)n < sizeof(struct msg_header)) break; //śmieci albo -1 czyli timeout

                struct msg_header* hdr = (struct msg_header*)buf;
                //odbiory
                if(hdr->type == MSG_PUNCH) {
                    if(hdr->payload_len == 0 && 
                      (net_addr_compare(&host_addr_public, &sender) ||
                      (net_addr_compare(&host_addr_local, &sender)))) { 
                        //faktyczny punch od hosta
                        //ok (y)
                        stan = PEER_STATE_CONNECTED;
                        host_addr_used = net_addr_compare(&host_addr_public, &sender) ? host_addr_public : host_addr_local;
                    } else { //informacja zeby zrobic punch
                        struct payload_punch* data = (struct payload_punch*)hdr->payload;

                        memset(&host_addr_local, 0, sizeof(host_addr_local));
                        memset(&host_addr_public, 0, sizeof(host_addr_public));
                        host_addr_public.sin_family = AF_INET;
                        host_addr_local.sin_family = AF_INET;
                        host_addr_public.sin_addr.s_addr = data->public_addr.sin_addr.s_addr;
                        host_addr_local.sin_addr.s_addr = data->local_addr.sin_addr.s_addr;
                        host_addr_public.sin_port = data->public_addr.sin_port;
                        host_addr_local.sin_port = data->local_addr.sin_port;
                    }
                } else if(hdr->type == MSG_ERROR) {
                    printf("%s\n", ((struct payload_error*)hdr->payload)->message);
                    stan = PEER_STATE_BROWSING;
                }
                //wyslanie co 3 sekundy od kiedy znamy hosta (wystarczy sprawdzac public nawet jak wysylane na oba)
                if(host_addr_public.sin_addr.s_addr != 0) {
                    size_t len = build_frame(buf, MSG_PUNCH, NULL, 0);
                    tui_log(tui, "Wysylam punch do PUB: %s:%d family:%d", 
                        inet_ntoa(host_addr_public.sin_addr), 
                        ntohs(host_addr_public.sin_port),
                        host_addr_public.sin_family);
                    tui_log(tui, "Wysylam punch do LOC: %s:%d family:%d",
                        inet_ntoa(host_addr_local.sin_addr),
                        ntohs(host_addr_local.sin_port),
                        host_addr_local.sin_family);
                    net_send(sock, buf, len, &host_addr_public);
                    net_send(sock, buf, len, &host_addr_local);
                }
                break;
            }
            case PEER_STATE_CONNECTED: {
                tui_get_join(tui);
                peer_chat(sock, &host_addr_used, n, tui);
                return;
            } break;
        }
    }
}

void peer_chat(int sock, struct sockaddr_in *host, char* n, tui_t* tui) {
    uint8_t buf[BUF_SIZE];
 
    //wysyla CHAT_JOIN

    struct chat_payload_join join_pl;
    strncpy(join_pl.name, n, NICK_LEN - 1);
    join_pl.name[NICK_LEN - 1] = '\0';
    size_t len = build_frame(buf, CHAT_JOIN, &join_pl, sizeof(join_pl));
    net_send(sock, buf, len, host);
    net_set_timeout(sock, 0, 10000); //timeout do czatowania
 
    time_t last_punch = time(NULL);
    struct sockaddr_in sender;
 
    while(1) {
        //wyslanie puncha
        if(time(NULL) - last_punch >= 10) {
            size_t len = build_frame(buf, CHAT_PUNCH, NULL, 0);
            net_send(sock, buf, len, host);
            last_punch = time(NULL);
        }
 
        //odbiór wiadomości z sieci
        memset(buf, 0, BUF_SIZE);
        int n = net_recv(sock, buf, BUF_SIZE, &sender); //maks 10 ms bedzie
        if(n > 0 && (size_t)n >= sizeof(struct msg_header)) {
            struct msg_header *hdr = (struct msg_header *)buf;
            tui_log(tui, "Przyszla wiadomosc, typ: %d", hdr->type);
            switch(hdr->type) {
                case CHAT_MSG: {
                    tui_log(tui, "CHAT_MSG od %s:%d", inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
                    if(ntohs(hdr->payload_len) < sizeof(struct chat_payload_msg)) break;
                    struct chat_payload_msg *pl = (struct chat_payload_msg *)hdr->payload;
                    tui_on_msg(tui, pl->name, pl->mess);
                    break;
                }
                case CHAT_JOIN: {
                    if(ntohs(hdr->payload_len) < sizeof(struct chat_payload_join)) break;
                    struct chat_payload_join *pl = (struct chat_payload_join *)hdr->payload;
                    tui_on_join(tui, pl->name);
                    break;
                }
                case CHAT_LEAVE: {
                    tui_on_leave(tui, "???");
                    break;
                }
                case CHAT_KICK: {
                    tui_on_kick(tui);
                    //poinformuj hosta że wychodzimygrzecznie 
                    len = build_frame(buf, CHAT_LEAVE, NULL, 0);
                    net_send(sock, buf, len, host);
                    return;
                }
                default: break;
            }
        }
 
        //wejście z klawiatury, jesli true to byl enter i wysylamy
        if(tui_process_input(tui)) {
            if(tui->input_buf[0] == '\0') continue;
 
            if(strcmp(tui->input_buf, "exit") == 0) {
                len = build_frame(buf, CHAT_LEAVE, NULL, 0);
                net_send(sock, buf, len, host);
                return;
            }
 
            struct chat_payload_msg data;
            strncpy(data.name, tui->user_data.nick, NICK_LEN - 1);
            data.name[NICK_LEN - 1] = '\0';
            strncpy(data.mess, tui->input_buf, MESS_LEN - 1);
            data.mess[MESS_LEN - 1] = '\0';
            len = build_frame(buf, CHAT_MSG, &data, sizeof(data));
            net_send(sock, buf, len, host);
            tui_on_msg(tui, data.name, data.mess);
            tui_get_send(tui);
        }
    }
}