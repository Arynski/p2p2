#include "room.h"
#include <stdlib.h>
#include <syslog.h>
struct room rooms[MAX_ROOMS];

int room_add(const char *name, struct sockaddr_in *host_addr) {
    //jeden host -- jeden pokój!!!, usun stary pokoj
    int old = room_find_by_host(host_addr);
    if(old != -1) memset(&rooms[old], 0, sizeof(struct room));

    short duplicate = 0;
    int slot = -1;
    int i = room_count();
    if(i < MAX_ROOMS) {
        struct room new;
        new.active = 1;
        new.host_addr = *host_addr;
        new.last_ping = time(NULL);
        strncpy(new.name, name, sizeof(new.name) - 1);
        new.name[sizeof(new.name)-1] = '\0';

        for (int j = 0; j < MAX_ROOMS; j++) {
           if (!rooms[j].active) { slot = j; break; }
        }

        do {
            duplicate = 0;
            new.id = (rand()%10000);
            for(int j = 0; j < MAX_ROOMS; ++j) {
                if(rooms[j].id == new.id && rooms[j].active) {
                    duplicate = 1;
                    break;
                }
            }
        } while(duplicate);
        rooms[slot] = new;
    }
    return slot;
}

void room_remove(uint32_t id) {
    for(int i = 0; i < MAX_ROOMS; ++i) {
        if(rooms[i].id == id) {
            rooms[i].active = 0;
            break;
        }
    }
}

struct room *room_find(uint32_t id) {
    for(int i = 0; i < MAX_ROOMS; ++i) {
        if(rooms[i].active && rooms[i].id == id)
            return &rooms[i];
    }
    return NULL;
}

int room_count(void) {
    int count = 0;
    for(int i = 0; i < MAX_ROOMS; ++i) {
        if(rooms[i].active == 1)
            count++;
    }
    return count;
}

void room_cleanup_expired(time_t timeout) {
    for(int i = 0; i < MAX_ROOMS; ++i) {
        if(rooms[i].active && (time(NULL) - rooms[i].last_ping > timeout)) {
            syslog(LOG_INFO, "Usuwam pokoj o nazwie \'%s\' za nieaktywnosc", rooms[i].name);
            memset(&rooms[i], 0, sizeof(struct room));
        }
    }
}

int room_find_by_host(struct sockaddr_in *host_addr) {
    for(int i = 0; i < MAX_ROOMS; ++i) {
        if(rooms[i].host_addr.sin_port == host_addr->sin_port &&
           rooms[i].host_addr.sin_addr.s_addr == host_addr->sin_addr.s_addr)
            return i;
    }
    return -1;
}

int room_get_active(struct room_entry* active_rooms, int size) {
    int cnt = 0;
    for(int i = 0; i < MAX_ROOMS; ++i) {
        if(rooms[i].active && cnt < size) {
            strncpy(active_rooms[cnt].name, rooms[i].name, sizeof(active_rooms[cnt].name) - 1);
            active_rooms[cnt].room_id = rooms[i].id;
            cnt++;
        }
    }
    return cnt;
}