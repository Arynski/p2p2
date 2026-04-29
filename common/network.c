#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <ifaddrs.h>
#include "network.h"

int net_init(uint16_t port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return -1; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; //ipv4
    addr.sin_port = htons(port); //port
    addr.sin_addr.s_addr = INADDR_ANY; //adres ip, same zera -- wszystkie interfejsy

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }

    return sock;
}

int net_send(int sock, const void *buf, size_t len, struct sockaddr_in *dest) {
    int n = sendto(sock, buf, len, 0, (struct sockaddr*)dest, sizeof(*dest));
    if (n < 0) perror("sendto");
    return n;
}

int net_recv(int sock, void *buf, size_t len, struct sockaddr_in *sender) {
    socklen_t addrlen;
    struct sockaddr_in tmp;

    if(sender == NULL) {
        sender = &tmp;
    }

    addrlen = sizeof(*sender);

    int n = recvfrom(sock, buf, len, 0, (struct sockaddr*)sender, &addrlen);

    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recvfrom");
        }
    }

    return n;
}

void net_close(int sock) {
    close(sock);
}

int net_addr_compare(struct sockaddr_in* addr1, struct sockaddr_in* addr2) {
    if(addr1 != NULL && addr2 != NULL)
        return (addr1->sin_addr.s_addr == addr2->sin_addr.s_addr 
                && addr1->sin_port == addr2->sin_port);
    else
        return 0;
}   

void net_set_timeout(int sock, int ts, int tu) {
    struct timeval tv;
    tv.tv_sec = ts;
    tv.tv_usec = tu;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

struct sockaddr_in net_get_local_sockaddr(int sock) {
    struct sockaddr_in result = {0};
    struct ifaddrs *ifaddr, *ifa;
    uint32_t ip = 0;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return result;
    }

    //dla kazdego interfejsu, zwroci pierwszy wiec tu jest teoretycznie mozliwosc ze ktos ma
    //podpiety kabel np i wifi (wifi z internetem, kabel bez) i to znajdzie kabel i siedzi w limboo000
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;

        //dzialamy w ipv4
        if (ifa->ifa_addr->sa_family == AF_INET) {
            if (strcmp(ifa->ifa_name, "lo") != 0) {
                struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
                
                //to juz jest network byte order
                ip = addr->sin_addr.s_addr;
                break; 
            }
        }
    }
    freeifaddrs(ifaddr);

    //pobranie portu
    uint16_t my_port;
    struct sockaddr_in sin;
    socklen_t s_len = sizeof(sin);
    if (getsockname(sock, (struct sockaddr *)&sin, &s_len) == -1) {
        perror("getsockname");
    } else {
        my_port = sin.sin_port; 
    }

    result.sin_addr.s_addr = ip;
    result.sin_family = AF_INET;
    result.sin_port = my_port;

    return result;
}