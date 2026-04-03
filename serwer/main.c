#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include "room.h"
#include "common/network.h"
#include "handler.h"
#include "common/protocol_STUN.h"

#define PORT 8888
#define BUF_SIZE 1024

int main() {
    openlog("stun-server", LOG_PID | LOG_NDELAY, LOG_DAEMON);
    syslog(LOG_INFO, "Serwer startuje na porcie %d", PORT);

    srand(time(NULL));
    int sock = net_init(PORT);
    if(sock == -1) {
        syslog(LOG_ERR, "Nie udalo sie zainicjalizowac gniazda na porcie %d", PORT);
        closelog();
        return 1;
    }

    syslog(LOG_INFO, "Serwer nasluchuje na porcie %d", PORT);

    uint8_t buf[BUF_SIZE];
    struct sockaddr_in sender;
    time_t last_cleanup = time(NULL);
    net_set_timeout(sock, 10);

    while(1) {
        int n = net_recv(sock, buf, BUF_SIZE, &sender);
        //w wypadku timeoutu net_recv zwraca -1 i wtedy nie powinno byc continue, ale moze tez jaki error
        if (n > 0) {
            if (n >= (int)sizeof(struct msg_header)) {
                struct msg_header *hdr = (struct msg_header *)buf;
                handle_payload(sock, &sender, hdr);
            } else {
                syslog(LOG_WARNING, "Otrzymano za krótki pakiet od %s", inet_ntoa(sender.sin_addr));
            }
        } else if (n < 0) {
            //albo blad albo timeout
            if (errno != EAGAIN && errno != EWOULDBLOCK) { syslog(LOG_ERR, "Blad odczytu z sieci: %m"); }
        }

        if(time(NULL) - last_cleanup >= 30) {
            room_cleanup_expired(TIMEOUT_ROOM); // usuwa pokoje bez pingu przez 15 sekund
            last_cleanup = time(NULL);
        }
    }

    net_close(sock);
    closelog();
    return 0;
}