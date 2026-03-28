#include <stdio.h>
#include <string.h>
#include "peer.h"
#include "common/network.h"
#include "handler.h"
#include "common/protocol_mess.h"
#include <sys/select.h>

void peer_start(int sock, struct sockaddr_in *server) {
    char nick[NICK_LEN];
    uint8_t buf[BUF_SIZE];
    peer_state_t stan = PEER_STATE_START;
    uint8_t lista_buf[sizeof(struct payload_list_resp) + sizeof(struct room_entry) * MAX_ROOMS];
    struct payload_list_resp *pokoje = (struct payload_list_resp *)lista_buf;
    struct sockaddr_in host_addr; host_addr.sin_addr.s_addr = 0;
    while(1) {
        printf("stan: %d\n", stan);
        fflush(stdout);
        switch(stan) {
            case PEER_STATE_START: {
                printf("Start peera!\n");
                printf("Podaj nick: ");
                fgets(nick, sizeof(nick), stdin);
                nick[strcspn(nick, "\n")] = 0; // usuwa newline na końcu
                printf("Pobieram listę pokojów z serwera...\n");
                size_t len = build_frame(buf, MSG_LIST, NULL, 0);
                net_send(sock, buf, len, server);
                stan = PEER_STATE_WAITING_LIST;
                break;
            }       
            case PEER_STATE_WAITING_LIST: {
                int n = net_recv(sock, buf, BUF_SIZE, NULL);
                if(n < sizeof(struct msg_header)) break; //śmieci
                struct msg_header* hdr = (struct msg_header*)buf;
                if(hdr->type == MSG_LIST_RESP) {
                    printf("Serwer odpowiedział!\n");
                    memcpy(lista_buf, hdr->payload, hdr->payload_len);
                    if(pokoje->count == 0)
                        printf("Brak pokojów!\n");
                    else {
                        for(int i = 0; i < pokoje->count; ++i) {
                            printf("%d. pokój %s | %d\n", i,
                                                    pokoje->rooms[i].name, 
                                                    pokoje->rooms[i].room_id);
                        }
                    }
                    stan = PEER_STATE_BROWSING;
                }
                else if(hdr->type == MSG_ERROR) {
                    printf("Błąd!\n");
                }
                break;
            }
            case PEER_STATE_BROWSING: {
                int wybor;
                printf("Do ktorego pokoju dołączyć? Podaj numer: ");
                scanf("%d", &wybor);
                while(getchar() != '\n');
                
                struct payload_join data;
                data.room_id = pokoje->rooms[wybor].room_id;
                size_t len = build_frame(buf, MSG_JOIN, &data, sizeof(data));
                net_send(sock, buf, len, server);

                net_set_timeout(sock, 3); //timeout do laczenia sie
                stan = PEER_STATE_CONNECTING;
                break;
            }
            case PEER_STATE_CONNECTING: {
                printf("Łączenie...\n");
                fflush(stdout);
                //ewentualne problemy jakie tutaj sa to takie, ze 
                struct sockaddr_in sender;
                int n = net_recv(sock, buf, BUF_SIZE, &sender); //maks 3 sekundy bedzie
                printf("net_recv zwrocil: %d\n", n);
                fflush(stdout);
                if(n < sizeof(struct msg_header)) break; //śmieci albo -1 czyli timeout

                struct msg_header* hdr = (struct msg_header*)buf;
                //odbiory
                if(hdr->type == MSG_PUNCH) {
                    if(hdr->payload_len == 0 && net_addr_compare(&host_addr, &sender)) { 
                        //faktyczny punch od hosta
                        //ok (y)
                        printf("Odebrano punch od hosta...");
                        stan = PEER_STATE_CONNECTED;
                    } else { //informacja zeby zrobic punch
                        printf("Otrzymalem informacje z serwera...");
                        struct payload_punch* data = (struct payload_punch*)hdr->payload;
                        host_addr = data->addr;
                    }
                } else if(hdr->type == MSG_ERROR) {
                    printf("%s\n", ((struct payload_error*)hdr->payload)->message);
                    stan = PEER_STATE_BROWSING;
                }
                //wyslanie co 3 sekundy od kiedy znamy hosta
                if(host_addr.sin_addr.s_addr != 0) {
                    printf("Wysylam punch...");
                    size_t len = build_frame(buf, MSG_PUNCH, NULL, 0);
                    net_send(sock, buf, len, &host_addr);
                }
                break;
            }
            case PEER_STATE_CONNECTED: {
                printf("Brawo, polaczono!");
                peer_chat(sock, &host_addr, nick);
                return;
            } break;
        }
    }
}

void peer_chat(int sock, struct sockaddr_in *host, char* nick) {
    uint8_t buf[BUF_SIZE];
 
    //wysyla CHAT_JOIN

    struct chat_payload_join join_pl;
    strncpy(join_pl.name, nick, NICK_LEN - 1);
    join_pl.name[NICK_LEN - 1] = '\0';
    size_t len = build_frame(buf, CHAT_JOIN, &join_pl, sizeof(join_pl));
    net_send(sock, buf, len, host);
 
    printf("Jesteś na czacie jako '%s'. Wpisz 'exit' aby wyjść.\n", nick);
    fflush(stdout);
 
    fd_set readfds;
    int maxfd = sock > 0 ? sock : 0;
    time_t last_punch = time(NULL);
 
    while(1) {
        struct timeval tv = {1, 0}; // 1 sekunda — żeby pętla nie wisiała wiecznie
        FD_ZERO(&readfds);
        FD_SET(0, &readfds);    // stdin
        FD_SET(sock, &readfds); // socket
 
        if(select(maxfd + 1, &readfds, NULL, NULL, &tv) < 0) {
            perror("select");
            break;
        }

        //wyslanie puncha
        if(time(NULL) - last_punch >= 10) {
            size_t len = build_frame(buf, CHAT_PUNCH, NULL, 0);
            net_send(sock, buf, len, host);
            last_punch = time(NULL);
        }
 
        //odbiór wiadomości z sieci
        if(FD_ISSET(sock, &readfds)) {
            struct sockaddr_in sender;
            int n = net_recv(sock, buf, BUF_SIZE, &sender);
            if(n < (int)sizeof(struct msg_header)) continue;
            struct msg_header *hdr = (struct msg_header *)buf;
 
            switch(hdr->type) {
                case CHAT_MSG: {
                    if(hdr->payload_len < sizeof(struct chat_payload_msg)) break;
                    struct chat_payload_msg *pl = (struct chat_payload_msg *)hdr->payload;
                    printf("\r%-40s\n", " "); 
                    printf("%s: %s\n", pl->name, pl->mess);
                    printf("> "); fflush(stdout);
                    break;
                }
                case CHAT_JOIN: {
                    if(hdr->payload_len < sizeof(struct chat_payload_join)) break;
                    struct chat_payload_join *pl = (struct chat_payload_join *)hdr->payload;
                    printf("\r%-40s\n", " ");
                    printf("*** %s dołączył do pokoju ***\n", pl->name);
                    printf("> "); fflush(stdout);
                    break;
                }
                case CHAT_LEAVE: {
                    printf("\r%-40s\n", " ");
                    printf("*** ktoś wyszedł z pokoju ***\n");
                    printf("> "); fflush(stdout);
                    break;
                }
                case CHAT_KICK: {
                    printf("\nZostałeś wyrzucony z pokoju.\n");
                    // poinformuj hosta że wychodzimy
                    len = build_frame(buf, CHAT_LEAVE, NULL, 0);
                    net_send(sock, buf, len, host);
                    return;
                }
                default: break;
            }
        }
 
        //wejście z klawiatury
        if(FD_ISSET(0, &readfds)) {
            char message[MESS_LEN];
            if(fgets(message, sizeof(message), stdin) == NULL) break;
            message[strcspn(message, "\n")] = '\0';
 
            if(strcmp(message, "exit") == 0) {
                printf("Opuszczasz pokój...\n");
                len = build_frame(buf, CHAT_LEAVE, NULL, 0);
                net_send(sock, buf, len, host);
                return;
            }
 
            if(message[0] == '\0') {
                printf("> "); fflush(stdout);
                continue;
            }
 
            struct chat_payload_msg data;
            strncpy(data.name, nick, NICK_LEN - 1);
            data.name[NICK_LEN - 1] = '\0';
            strncpy(data.mess, message, MESS_LEN - 1);
            data.mess[MESS_LEN - 1] = '\0';
            len = build_frame(buf, CHAT_MSG, &data, sizeof(data));
            net_send(sock, buf, len, host);
 
            printf("> "); fflush(stdout);
        }
    }
}