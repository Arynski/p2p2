#include <stdio.h>
#include <string.h>
#include <time.h>
#include "peer.h"
#include "common/network.h"
#include "handler.h"
#include "common/protocol_mess.h"
#include "tui/tui.h"
#include <sys/select.h>

void peer_start(int sock, struct sockaddr_in *server, char* n, tui_t* tui, uint8_t* peer_pub, uint8_t* peer_sec) {
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
                    if(tui->mode == TUI_MENU) return; //powrot
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

                        host_addr_public.sin_addr.s_addr = data->public_ip;
                        host_addr_local.sin_addr.s_addr = data->local_ip;
                        host_addr_public.sin_port = data->public_port;
                        host_addr_local.sin_port = data->local_port;
                    }
                } else if(hdr->type == MSG_ERROR) {
                    printf("%s\n", ((struct payload_error*)hdr->payload)->message);
                    stan = PEER_STATE_BROWSING;
                }
                break;
            }
            case PEER_STATE_CONNECTED: {
                peer_chat(sock, &host_addr_used, n, tui, peer_pub, peer_sec);
                return;
            } break;
        }
    }
}

void peer_chat(int sock, struct sockaddr_in *host, char* n, tui_t* tui, uint8_t* peer_pub, uint8_t* peer_sec) {
    uint8_t buf[BUF_SIZE];
    bool is_chatting = false;
    uint8_t session_key_rx[crypto_kx_SESSIONKEYBYTES];
    uint8_t session_key_tx[crypto_kx_SESSIONKEYBYTES];

    //wysyla CHAT_JOIN
    tui_log(tui, "Wysylam chat_join, typ %d", CHAT_JOIN);
    struct chat_payload_join join_pl;
    strncpy(join_pl.name, n, NICK_LEN - 1);
    join_pl.name[NICK_LEN - 1] = '\0';
    memcpy(join_pl.public_key, peer_pub, crypto_kx_PUBLICKEYBYTES);
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
            if(!is_chatting && hdr->type != CHAT_JOIN_OK)
                continue;
            switch(hdr->type) {
                case CHAT_MSG: {
                    tui_log(tui, "CHAT_MSG od %s:%d", inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
                    if(ntohs(hdr->payload_len) < sizeof(struct chat_payload_msg)) break;
                    struct chat_payload_msg *pl = (struct chat_payload_msg *)hdr->payload;
                    
                    //odszyfrowac
                    uint8_t nonce[crypto_stream_NONCEBYTES] = {0};
                    char decoded[MESS_LEN];
                    
                    crypto_stream_xor((uint8_t*)decoded, (const uint8_t*)pl->mess, 
                                    MESS_LEN, nonce, session_key_rx);
                    decoded[MESS_LEN - 1] = '\0';

                    tui_on_msg(tui, pl->name, decoded);
                    break;
                }
                 case CHAT_JOIN_OK: {
                    if(ntohs(hdr->payload_len) < sizeof(struct chat_payload_join_ok)) break;
                    struct chat_payload_join_ok *pl = (struct chat_payload_join_ok *)hdr->payload;
                    
                    if (crypto_kx_client_session_keys(session_key_rx, session_key_tx, 
                                                      peer_pub, peer_sec, pl->public_key) != 0) {
                        tui_log(tui, "Nie można wyliczyć klucza sesyjnego!");
                        break;
                    }

                    tui_get_join(tui);
                    is_chatting = true;
                    break;
                }
                case CHAT_JOIN: {
                    if(ntohs(hdr->payload_len) < sizeof(struct chat_payload_join)) break;
                    struct chat_payload_join *pl = (struct chat_payload_join *)hdr->payload;
                    tui_on_join(tui, pl->name);
                    break;
                }
                case CHAT_LEAVE: {
                    struct chat_payload_leave* pl = (struct chat_payload_leave*)hdr->payload;
                    tui_on_leave(tui, pl->name);
                    break;
                }
                case CHAT_KICK: {
                    tui_on_kick(tui);
                    //poinformuj hosta że wychodzimygrzecznie 
                    len = build_frame(buf, CHAT_LEAVE, NULL, 0);
                    net_send(sock, buf, len, host);
                    return;
                }
                case CHAT_CLOSE_ROOM: {
                    tui_on_close_room(tui);
                    break;
                }
                default: break;
            }
        }
 
        //wejście z klawiatury, jesli true to byl enter i wysylamy
        if(tui_process_input(tui)) {
            if(tui->input_buf[0] == '\0') continue;
 
            if(strcmp(tui->input_buf, "/exit") == 0) {
                struct chat_payload_leave pl;
                strncpy(pl.name, tui->user_data.nick, NICK_LEN - 1);
                pl.name[NICK_LEN - 1] = '\0';
                len = build_frame(buf, CHAT_LEAVE, &pl, sizeof(pl));
                net_send(sock, buf, len, host);

                tui_exit_chat(tui);
                return;
            }
 
            struct chat_payload_msg data;
            strncpy(data.name, tui->user_data.nick, NICK_LEN - 1);
            data.name[NICK_LEN - 1] = '\0';
            memset(data.mess, 0, MESS_LEN);

            //szyfrowac
            uint8_t nonce[crypto_stream_NONCEBYTES] = {0};
            crypto_stream_xor((uint8_t*)data.mess, (const uint8_t*)tui->input_buf, 
                            strlen(tui->input_buf) + 1, nonce, session_key_tx);

            len = build_frame(buf, CHAT_MSG, &data, sizeof(data));
            net_send(sock, buf, len, host);
            tui_on_msg(tui, data.name, tui->input_buf);
            tui_get_send(tui);
        }
    }
}