#include <stdio.h>
#include <string.h>
#include "peer.h"
#include "common/network.h"
#include "handler.h"
#include "common/protocol_mess.h"

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
                struct sockaddr_in sender;
                int n = net_recv(sock, buf, BUF_SIZE, &sender); //maks 3 sekundy bedzie
            } break;
        }
    }
}