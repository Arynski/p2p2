#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common/network.h"
#include "handler.h"
#include "host.h"
#include "peer.h"

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
    stun.sin_addr.s_addr = inet_addr("127.0.0.1");
    uint8_t buf[BUF_SIZE];

    while(1) {
        int czy_host = 0;
        printf("Czy jestes hostem? (1/0): ");
        scanf("%d", &czy_host); while(getchar() != '\n');

        if(czy_host) {
            host_start(sock, &stun);
        } else {
            peer_start(sock, &stun);
        }
    }
}