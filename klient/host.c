#include <stdio.h>
#include <string.h>
#include "host.h"
#include "common/network.h"
#include "handler.h"
#include "common/protocol_mess.h"
#include <sys/select.h>

void host_start(int sock, struct sockaddr_in *server, char* n, tui_t* tui) {
    uint8_t buf[BUF_SIZE];
    host_start_state_t stan = HOST_STATE_START;
    tui_log(tui, "HOST_STATE_START!");
    while(1) {
        switch(stan) {
            case HOST_STATE_START: {
                if(tui_process_input(tui)) { //czekamy na enter
                    //wyslanie register
                    
                    struct sockaddr_in local_ip = net_get_local_sockaddr(sock);
                    struct payload_register data = {0};
                    strncpy(data.name, tui->input_buf, 64);
                    data.name[63] = '\0'; // Bezpieczne zakończenie stringa
                    data.host_local_ip = local_ip.sin_addr.s_addr;
                    data.host_local_port = local_ip.sin_port; //z getsockname juz jest w dobrej kolejnosci (NBO)

                    size_t len = build_frame(buf, MSG_REGISTER, &data, sizeof(data));
                    net_send(sock, buf, len, server);
                    stan = HOST_STATE_WAITING_REGISTERED;
                    tui_log(tui, "HOST_STATE_WAITING_REGISTERED!");
                }
            } break;
            case HOST_STATE_WAITING_REGISTERED: {
                int n = net_recv(sock, buf, BUF_SIZE, NULL);
                if(n < sizeof(struct msg_header)) break;
                struct msg_header *hdr = (struct msg_header *)buf;
                if(hdr->type == MSG_REGISTERED) {
                    struct payload_registered *ack = (struct payload_registered *)hdr->payload;
                    tui_get_registered(tui);
                    stan = HOST_STATE_HOSTING;
                    tui_log(tui, "HOST_STATE_HOSTING!");
                } else if(hdr->type == MSG_ERROR) {
                    tui_log(tui, "registered blad!");
                    stan = HOST_STATE_START;
                }
            break;
            }
            case HOST_STATE_HOSTING:
                tui_log(tui, "DO NOWEJ FUNKCJI!");
                host_hosting(sock, server, n, tui);
                return;
        }
    }
}

void host_hosting(int sock, struct sockaddr_in *server, char* n, tui_t* tui) {
    uint8_t buf[BUF_SIZE];
    struct peer pending_peers[MAX_PEERS];  //w trakcie hole punching
    struct peer connected_peers[MAX_PEERS]; //połączeni
    memset(pending_peers, 0, sizeof(pending_peers));
    memset(connected_peers, 0, sizeof(connected_peers));
    int pending_count = 0;
    int connected_count = 0;

    struct sockaddr_in my_addr;
    socklen_t my_len = sizeof(my_addr);
    getsockname(sock, (struct sockaddr*)&my_addr, &my_len);

    /*powinno obslugiwac: 
        przychodzace wiadomosci i je rozsylac,
        swoje wiadomosci i je rozsylac,
        łączacych sie peerow 
    */

    net_set_timeout(sock, 0, 10000); //timeout do czatowania

    fd_set readfds;
    time_t last_ping = time(NULL);
    time_t last_keepalive = time(NULL);
    struct sockaddr_in sender;
    while(1) {
        memset(buf, 0, BUF_SIZE);
        int recv_n = net_recv(sock, buf, BUF_SIZE, &sender); // wraca po 10ms
        if(recv_n >= (int)sizeof(struct msg_header)) {
            struct msg_header *hdr = (struct msg_header *)buf;
            tui_log(tui, "n = %d", recv_n);
            tui_log(tui, "typ: %d", hdr->type);
            switch (hdr->type) {
                case MSG_PUNCH:     handle_hosting_punch(sock, &sender, hdr,
                                                         pending_peers, &pending_count,
                                                         connected_peers, &connected_count, tui); break;
                case CHAT_JOIN:     handle_chat_join(sock, &sender, hdr,
                                                      connected_peers, &connected_count, tui); break;
                case CHAT_LEAVE:    handle_chat_leave(sock, &sender, hdr,
                                                connected_peers, &connected_count, tui); break;
                case CHAT_MSG: {
                    if(hdr->payload_len < sizeof(struct chat_payload_msg)) break;
                    tui_log(tui, "CHAT_MSG od %s:%d", inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
                    handle_chat_msg(sock, &sender, hdr,
                                                connected_peers, &connected_count, tui);
                    struct chat_payload_msg *pl = (struct chat_payload_msg *)hdr->payload;
                    tui_on_msg(tui, pl->name, pl->mess);
                    break;
                } break;
                case CHAT_PUNCH:    handle_chat_punch(sock, &sender, hdr,
                                                connected_peers, connected_count); break;
            default: break;
            }   
        }

        if(tui_process_input(tui)) {
            if(tui->input_buf[0] == '\0') continue;
            if(strcmp(tui->input_buf, "exit") == 0) break;

            struct chat_payload_msg data;
            strncpy(data.name, n, NICK_LEN - 1);
            data.name[NICK_LEN - 1] = '\0';
            strncpy(data.mess, tui->input_buf, MESS_LEN - 1);
            data.mess[MESS_LEN - 1] = '\0';
            uint8_t msgbuf[sizeof(struct msg_header) + sizeof(data)];
            build_frame(msgbuf, CHAT_MSG, &data, sizeof(data));
            broadcast_mess(sock, connected_peers, msgbuf, NULL, tui);
            tui_on_msg(tui, n, tui->input_buf);
            tui_get_send(tui);
        }

        //tutaj wysyla do laczacy sie dopiero
        if(time(NULL) - last_ping >= 3) {
            send_punches(sock, pending_peers, tui);
            last_ping = time(NULL);
        }
        //tutaj wysyla do juz polaczonych zeby utrzymac dziure 
        if(time(NULL) - last_keepalive >= 15) {
            send_punches(sock, connected_peers, tui);
            size_t len = build_frame(buf, MSG_PING, NULL, 0);
            net_send(sock, buf, len, server);
            last_keepalive = time(NULL);
        }
    }
}

void send_punches(int sock, struct peer* who, tui_t* tui) {
    uint8_t buf[BUF_SIZE];
    size_t len = build_frame(buf, MSG_PUNCH, NULL, 0); 
    for(int i = 0; i < MAX_PEERS; ++i) {
        if(who[i].active) {
            if(who[i].used_addr.sin_addr.s_addr != 0) {
                //connected
                net_send(sock, buf, len, &who[i].used_addr);
            } else {
                //jeszcze pending
                net_send(sock, buf, len, &who[i].public_addr);
                net_send(sock, buf, len, &who[i].local_addr);
            }
            if(time(NULL) - who[i].timestamp >= TIMEOUT_PEER) {
                tui_log(tui, "Peer %s wyrzucony z powodu timeoutu\n", who[i].nick);
                memset(&who[i], 0, sizeof(struct peer));
            }
        }
    }
}

void broadcast_mess(int sock, struct peer* who, uint8_t* msg, struct sockaddr_in *sender, tui_t* tui) {
    size_t len = sizeof(struct msg_header) + ((struct msg_header*)msg)->payload_len;
    for(int i = 0; i < MAX_PEERS; ++i) {
        if(who[i].active && !net_addr_compare(sender, &who[i].used_addr)) {
            tui_log(tui, "Wysylam do i = %d", i);
            net_send(sock, msg, len, &who[i].used_addr);
        }
    }
}

void handle_hosting_punch(int sock, struct sockaddr_in *sender, struct msg_header *hdr,
                          struct peer *pending, int *pending_count,
                          struct peer *connected, int *connected_count,
                          tui_t* tui) {
    uint8_t resp[BUF_SIZE];
    struct payload_punch* data = (struct payload_punch*)hdr->payload;

    //jesli 0 to znaczy ze to od peera, ktory chce sie polaczyc a nie zero to od serwera
    if(hdr->payload_len == 0) {
        tui_log(tui, "Otrzymano jakis punch z: %d\n", ntohs(sender->sin_port));
        for(int i = 0; i < MAX_PEERS; ++i) {
            if(net_addr_compare(sender, &pending[i].public_addr) || 
                net_addr_compare(sender, &pending[i].local_addr)) {
                //po prostu dodajemy do connected jesli mozna, jak nie to go ignorujemy
                //w przyszlosci mozna go poinformowac, bo tu juz powinna byc dziura
                if((*connected_count) >= MAX_PEERS) { return; }
                struct sockaddr_in from = net_addr_compare(sender, &pending[i].public_addr) ? pending[i].public_addr : pending[i].local_addr;
                tui_log(tui, "Otrzymano punch z: %d\n", from.sin_port);
                fflush(stdout);
                for(int j = 0; j < MAX_PEERS; ++j) {
                    if(!connected[j].active) {
                        connected[j] = pending[i];
                        connected[j].timestamp = time(NULL);
                        connected[j].nick[0] = '\0';
                        connected[j].used_addr = from;
                        memset(&pending[i], 0, sizeof(struct peer));
                        (*pending_count)--; (*connected_count)++;
                        //dla pewnosci v, tak samo jak peer
                        size_t len = build_frame(resp, MSG_PUNCH, NULL, 0);
                        net_send(sock, resp, len, &connected[j].used_addr);
                        tui_log(tui, "Dodano go do connected!\n");
                        fflush(stdout);
                        break;
                    }
                }
            }
        }
    } else {
        //else ze dostalismy punch z SERWERA

        //jak za duzo penduje to ignorujemy go xd
        //no bo dziura nie jest otwarta to nie mamy jak sie skomunikowac chyba ze przez serwer
        if((*pending_count) >= MAX_PEERS) { return; } 
        for(int i = 0; i < MAX_PEERS; ++i) {
            if(!pending[i].active) {
                pending[i].active = 1;
                pending[i].local_addr = data->local_addr;
                pending[i].public_addr = data->public_addr;
                pending[i].timestamp = time(NULL);
                (*pending_count)++;
                break;
            }
        }
        tui_log(tui, "Serwer chce nas zsynchronizowac, mnie z:\n");
        tui_log(tui, "sin_family: %d\n", data->public_addr.sin_family);
        tui_log(tui, "PUB adres: %s\n", inet_ntoa(data->public_addr.sin_addr));
        tui_log(tui, "PUB port: %d\n", ntohs(data->public_addr.sin_port));
        tui_log(tui, "LOC adres: %s\n", inet_ntoa(data->local_addr.sin_addr));
        tui_log(tui, "LOC port: %d\n", ntohs(data->local_addr.sin_port));
        //przy okazji wyslemy pierwsze do tego do ktorego mamy wyslac, na oba adresy, pub i loc
        size_t len = build_frame(resp, MSG_PUNCH, NULL, 0);
        net_send(sock, resp, len, &(data->public_addr));
        net_send(sock, resp, len, &(data->local_addr));
    }
}

void handle_chat_join(int sock, struct sockaddr_in *sender, struct msg_header *hdr,
                      struct peer *connected, int *connected_count, tui_t* tui) {
    if(hdr->payload_len < sizeof(struct chat_payload_join)) return;
    struct chat_payload_join *pl = (struct chat_payload_join*)hdr->payload;

    for(int i = 0; i < MAX_PEERS; ++i) {
        if(connected[i].active && 
           net_addr_compare(sender, &connected[i].used_addr)) {
            strncpy(connected[i].nick, pl->name, NICK_LEN);
            connected[i].nick[NICK_LEN-1] = '\0';
            connected[i].timestamp = time(NULL);
            tui_log(tui, "Peer %s dołączył!\n", connected[i].nick);
            fflush(stdout);

            uint8_t buf[BUF_SIZE];
            build_frame(buf, CHAT_JOIN, pl, sizeof(*pl));
            broadcast_mess(sock, connected, buf, sender, tui);
            return;
        }
    }
    // brak miejsca
    tui_log(tui, "Brak miejsca na nowego peera\n"); fflush(stdout);
}

void handle_chat_msg(int sock, struct sockaddr_in *sender, struct msg_header *hdr,
                     struct peer *connected, int connected_count, tui_t* tui) {
    if(hdr->payload_len < sizeof(struct chat_payload_msg)) return;
    struct chat_payload_msg *pl = (struct chat_payload_msg*)hdr->payload;
    char full_msg[NICK_LEN + MESS_LEN + 4];
    for(int i = 0; i < MAX_PEERS; ++i) {
        if(connected[i].active && net_addr_compare(sender, &connected[i].used_addr)) {
            strncpy(pl->name, connected[i].nick, NICK_LEN - 1);
            pl->name[NICK_LEN - 1] = '\0';  
            tui_log(tui, "%s: %s\n", pl->name, pl->mess);
            fflush(stdout);
            // rozsyłamy oryginalną ramkę dalej
            broadcast_mess(sock, connected, (uint8_t*)hdr, sender, tui);
            return;
        }
    }
}

void handle_chat_leave(int sock, struct sockaddr_in *sender, struct msg_header *hdr,
                       struct peer *connected, int *connected_count, tui_t* tui) {
    for(int i = 0; i < MAX_PEERS; ++i) {
        if(connected[i].active && net_addr_compare(sender, &connected[i].used_addr)) {
            tui_log(tui, "Peer %s wyszedł!\n", connected[i].nick);
            fflush(stdout);
            // rozsyłamy informację o wyjściu
            uint8_t buf[BUF_SIZE];
            build_frame(buf, CHAT_LEAVE, NULL, 0);
            // przed zerowaniem zapisz adres, potem rozsyłamy do pozostałych
            memset(&connected[i], 0, sizeof(struct peer));
            (*connected_count)--;
            broadcast_mess(sock, connected, buf, sender, tui);
            return;
        }
    }
}

void handle_chat_punch(int sock, struct sockaddr_in *sender, struct msg_header *hdr,
                     struct peer *connected, int connected_count) {
    for(int i = 0; i < MAX_PEERS; ++i) {
        if(connected[i].active && net_addr_compare(sender, &connected[i].used_addr)) {
            connected[i].timestamp = time(NULL);
            break;
        }
    }                    
}