#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <arpa/inet.h>
#include "protocol_STUN.h"

/** Tworzy i binduje socket UDP na podanym porcie. Zwraca deskryptor lub -1*/
int net_init(uint16_t port);

/**Wysyła bufor do podanego adresu. Zwraca liczbę wysłanych bajtów lub -1*/
int net_send(int sock, const void *buf, size_t len, struct sockaddr_in *dest);

/**Odbiera pakiet, wypełnia sender. Zwraca liczbę odebranych bajtów lub -1*/
int net_recv(int sock, void *buf, size_t len, struct sockaddr_in *sender);

/**Zamyka socket*/
void net_close(int sock);

/*Porownuje czy to ten sam adres i port (1) albo ze nie (0)*/
int net_addr_compare(struct sockaddr_in* addr1, struct sockaddr_in* addr2);

/*Ustawia timeout na podana ilosc sekund*/
void net_set_timeout(int sock, int t);

#endif