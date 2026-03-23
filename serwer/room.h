#ifndef ROOM_H
#define ROOM_H
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include "common/protocol_STUN.h"

#define TIMEOUT_ROOM 30

struct room {
    uint32_t        id;
    char            name[64];
    struct sockaddr_in host_addr;
    time_t          last_ping;
    int             active;
};
extern struct room rooms[MAX_ROOMS];


/*Zapisuje w tablicy rooms o rozmiarze MAX_ROOMS nowy pokoj o unikalnym ID
na pierwszym wolnym miejscu (active=0), zwraca pozycje na której zapisał lub -1*/
int room_add(const char *name, struct sockaddr_in *host_addr);

/*Usuwa pokój o podanym ID (ustawia active = 0)*/
void room_remove(uint32_t id);

/*Wyszukuje aktywny pokój o podanym id i zwraca ten pokój lub NULL*/
struct room *room_find(uint32_t id);

/*Zwraca liczbę aktywnych pokojów*/
int room_count(void);

/*Dezaktywuje pokoje o czasie pingu < timeout*/
void room_cleanup_expired(time_t timeout);

/*Zwraca indeks pokoju, którego hostem jest host_addr lub -1*/
int room_find_by_host(struct sockaddr_in *host_addr);

/*Zwraca ilość aktywnych pokoi oraz wypełnia otrzymany bufor*/
int room_get_active(struct room_entry* active_rooms, int size);

#endif