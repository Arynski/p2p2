#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common/network.h"
#include "handler.h"
#include "host.h"
#include "peer.h"
#include "tui/tui.h"

int main(int argc, char** argv) {
    int port;
    if(argc != 2) {
        printf("Proszę podać port i tylko port!\n");
        return 0;
    } else {
        port = atoi(argv[1]);
        printf("Korzystam z portu %d\n", port);
    }

    int sock = net_init(port);
    //test na lokalnym komputerze potem na serwerze zmienic tu vvv
    struct sockaddr_in stun;
    stun.sin_family = AF_INET;
    stun.sin_port = htons(8888);
    stun.sin_addr.s_addr = inet_addr("64.176.65.254");
    //stun.sin_addr.s_addr = inet_addr("127.0.0.1");
    //uint8_t buf[BUF_SIZE];

    tui_t okno;
    tui_init(&okno);

    while(1) {
        if(tui_process_input(&okno)) {
            if(okno.mode == TUI_LISTING || okno.mode == TUI_CREATE) 
                break; 
        }
    }

    char* nick = okno.user_data.nick;
    while(1) {
        if(okno.user_data.mode == 0) {
            host_start(sock, &stun, nick, &okno);
        } else if(okno.user_data.mode == 1) {
            peer_start(sock, &stun, nick, &okno);
        } else if(okno.mode == TUI_EXIT) {
            break;
        }
    }
}