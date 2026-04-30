#ifndef WRAPPING_H
#define WRAPPING_H

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct {
    int line_offsets[32]; //indeksy w ktorych zaczynaja sie nowe linie
    int line_lengths[32]; //dlugosc kazdej z linii
    int count;            //ile jest lini
} wrapped_info_t;

/*oblicza gdzie powinien byc zawiniety tekst, tworzy strukture 
wrapped_info, wypelnia ja i zwraca*/
wrapped_info_t wrap_text(const char *msg, int max_w) {
    wrapped_info_t info;
    info.count = 0;
    
    int msg_len = strlen(msg);
    int start = 0; //początek aktualnej linii

    while (start < msg_len && info.count < 32) {
        int end = start + max_w; //wymuszony koniec linii
        
        //jak cala reszta tekstu sie miesci to konczymy
        if (end >= msg_len) {
            info.line_offsets[info.count] = start;
            info.line_lengths[info.count] = msg_len - start;
            info.count++;
            break;
        }

        //cofamy sie od wymuszonego konca linii szukajac spacji
        int break_at = end;
        while (break_at > start && msg[break_at] != ' ') {
            break_at--;
        }

        //nie ma spacji to tniemy na chama
        if (break_at == start) {
            break_at = end;
        }

        //nowa linia
        info.line_offsets[info.count] = start;
        info.line_lengths[info.count] = break_at - start;
        info.count++;

        start = break_at;
        while (msg[start] == ' ') start++; 
    }

    return info;
}

#endif