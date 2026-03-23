#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
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
    socklen_t addrlen = sizeof(*sender);
    int n = recvfrom(sock, buf, len, 0, (struct sockaddr*)sender, &addrlen);
    if (n < 0) perror("recvfrom");
    return n;
}

void net_close(int sock) {
    close(sock);
}

int net_addr_compare(struct sockaddr_in* addr1, struct sockaddr_in* addr2) {
    return (addr1->sin_addr.s_addr == addr2->sin_addr.s_addr 
            && addr1->sin_port == addr2->sin_port);
}

void net_set_timeout(int sock, int t) {
    struct timeval tv;
    tv.tv_sec = t;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}