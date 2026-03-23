#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "room.h"
#include "common/network.h"
#include "handler.h"
#include "common/protocol_STUN.h"

#define PORT 8888
#define BUF_SIZE 1024

int main() {
    srand(time(NULL));
    int sock = net_init(PORT);
    if(sock == -1) return 1;

    printf("Serwer nasluchuje na porcie %d\n", PORT);

    uint8_t buf[BUF_SIZE];
    struct sockaddr_in sender;
    time_t last_cleanup = time(NULL);
    net_set_timeout(sock, 10);

    while(1) {
        int n = net_recv(sock, buf, BUF_SIZE, &sender);
        if(n < (int)sizeof(struct msg_header)) continue; // za krotki pakiet

        struct msg_header *hdr = (struct msg_header *)buf;
        handle_payload(sock, &sender, hdr);

        if(time(NULL) - last_cleanup >= 30) {
            room_cleanup_expired(TIMEOUT_ROOM); // usuwa pokoje bez pingu przez 15 sekund
            last_cleanup = time(NULL);
        }
    }

    net_close(sock);
    return 0;
}