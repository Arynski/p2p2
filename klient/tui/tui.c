#define _POSIX_C_SOURCE 200112L //dla setenv
#include "klient/tui/tui.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include "../../common/protocol_mess.h"
#include "../../common/protocol_STUN.h"
#include "wrapping.h"
#define UNUSED(x) (void)(x) //zeby kompilator nie krzyczal a funkcje mogly miec ladne interfejsy

void tui_init(tui_t* tui) {
    setenv("ESCDELAY", "25", 1);
    initscr();
    cbreak();
    noecho();
    int height = LINES;
    int width = COLS;

    /*Czat po lewej, na dole pole do wpisywania, z prawej logi (1.618)*/
    int chat_x = (float)width*0.618; int chat_y = height-4;
    int input_x = chat_x; int input_y = 4;
    int log_x = width-chat_x; int log_y = height;

    *tui = (tui_t){
        .chat_win = newwin(chat_y, chat_x, 0, 0),
        .log_win = newwin(log_y, log_x, 0, chat_x),
        .input_win = newwin(input_y, input_x, chat_y, 0),
        .input_len = 0,
        .cursor = 0,
        .option = 0,
        .num_options = 0,
        .mode = TUI_START, 
        .user_data = (user_t){
            .mode = 0,
        },
        .full = false,
        .head = 0,
    };
    memset(tui->input_buf, 0, MESS_LEN);
    tui->full = false;
    box(tui->chat_win, 0, 0); box(tui->log_win, 0, 0); box(tui->input_win, 0, 0);

    //okno logow
    int log_h = getmaxy(tui->log_win); 
    scrollok(tui->log_win, TRUE);
    wsetscrreg(tui->log_win, 1, log_h - 2);
    idlok(tui->log_win, TRUE);

    //czat
    scrollok(tui->chat_win, FALSE);

    keypad(tui->input_win, TRUE);
    refresh();
    wrefresh(tui->chat_win); wrefresh(tui->log_win); wrefresh(tui->input_win);
    nodelay(tui->input_win, 1);
    tui->draw_current = tui_draw_start_screen;
    tui->draw_current(tui);
}

uint8_t tui_process_input(tui_t* tui) {
    int in = wgetch(tui->input_win);
    switch(in) {
        case ERR: break;
        case KEY_ENTER: 
        case '\n': { 
            tui_log(tui, "ENTER wykryty, in=%d", in);
            switch(tui->mode) {
                case TUI_START: {
                    strncpy(tui->user_data.nick, tui->input_buf, NICK_LEN - 1);
                    tui->user_data.nick[NICK_LEN - 1] = '\0';
                    tui->mode = TUI_MENU;
                    memset(tui->input_buf, 0, MESS_LEN);
                    tui->input_len = 0;
                    tui->cursor = 0;
                    tui->num_options = 3;
                    tui->draw_current = tui_draw_menu;
                } break;
                case TUI_MENU: {
                    switch(tui->option) {
                        case 0: tui->user_data.mode = 1; tui->rooms_count = -1; tui->draw_current = tui_draw_loading; tui->loading = true; tui->mode = TUI_LISTING; break;
                        case 1: tui->user_data.mode = 0; tui->mode = TUI_CREATE; tui->draw_current = tui_draw_create; break;
                        case 2: tui->mode = TUI_EXIT; break;
                    }
                } break;
                case TUI_LISTING: if(!tui->loading) { tui->mode = TUI_CHAT; tui->draw_current = tui_draw_loading; tui->loading = true; } break;
                case TUI_CREATE: if(!tui->loading) { tui->mode = TUI_CHAT; tui->draw_current = tui_draw_loading; tui->loading = true; } break;
                case TUI_EXIT: break;
                case TUI_CHAT: break;
            }
            tui_handle_mode(tui);
            return true;    
        }
        case KEY_BACKSPACE: {
            if(tui->cursor <= 0)
                break; 
            memmove(&tui->input_buf[tui->cursor - 1], &tui->input_buf[tui->cursor], 
                tui->input_len - tui->cursor + 1);
            tui->input_len--;
            tui->cursor--;
        } break;
        case KEY_RIGHT: {
            if(tui->input_buf[tui->cursor] != '\0')
                tui->cursor++;
        } break;
        case KEY_LEFT: {
            if(tui->cursor != 0) {
                tui->cursor--;
            }
        } break;
        case KEY_DOWN: {
            if(tui->option < tui->num_options-1) {
                tui->option++;
                tui->draw_current(tui);
            }
        } break;
        case KEY_UP: {
            if(tui->option > 0) {
                tui->option--;
                tui->draw_current(tui);
            }
        } break;
        default: {
            if(in >= 32 && in <= 126 && tui->input_len < MESS_LEN - 1) {
                memmove(&tui->input_buf[tui->cursor+1], &tui->input_buf[tui->cursor],
                    tui->input_len - tui->cursor + 1);
                tui->input_buf[tui->cursor] = in;
                tui->input_len++;
                tui->cursor++;
            }
        } break;
    }

    wmove(tui->input_win, 1, 1);
    wclrtoeol(tui->input_win);
    
    mvwprintw(tui->input_win, 1, 1, "> %s", tui->input_buf);
    
    box(tui->input_win, 0, 0);
    wmove(tui->input_win, 1, tui->cursor + 3);
    
    wrefresh(tui->input_win);
    return false;
}

void tui_on_msg(tui_t* tui, const char *nick, const char *mess) {
    //tui_log(tui, "jestem w tui_on_msg!");
    if(tui->mode == TUI_CHAT) {
        chat_entry_t new_mess;
        new_mess.system = false;
        strncpy(new_mess.nick, nick, NICK_LEN - 1);
        new_mess.nick[NICK_LEN - 1] = '\0';
        strncpy(new_mess.msg, mess, MESS_LEN - 1);
        new_mess.msg[MESS_LEN - 1] = '\0';

        tui_add_mess(tui, new_mess);
        tui->draw_current(tui);
    }
}

void tui_on_join(tui_t* tui, const char *nick) {
    if(tui->mode == TUI_CHAT) {
        chat_entry_t new_mess;
        new_mess.system = true;
        snprintf(new_mess.nick, NICK_LEN, "system");
        
        // Składamy stringa bezpośrednio w docelowym miejscu
        snprintf(new_mess.msg, MESS_LEN, "Peer %s dolaczyl!", nick);

        tui_add_mess(tui, new_mess);
        tui->draw_current(tui);
    }
}

void tui_on_leave(tui_t* tui, const char *nick) {
    UNUSED(tui); UNUSED(nick);
}

void tui_on_kick(tui_t* tui) {
    UNUSED(tui);
}

void tui_on_frame(tui_t* tui, const char *dir, uint8_t type) {
    UNUSED(tui); UNUSED(dir); UNUSED(type);
}


void tui_get_list(tui_t* tui, struct payload_list_resp *pokoje) {
    tui_log(tui, "tutaj tui_get_list!");
    //tui jest w stanie listing i przestaje ładować, dostaje liste
    if(tui->mode == TUI_LISTING && tui->loading) {
        tui->rooms_count = pokoje->count;
        if (tui->rooms_count > MAX_ROOMS) tui->rooms_count = MAX_ROOMS;
        memcpy(tui->rooms, pokoje->rooms, tui->rooms_count * sizeof(struct room_entry));
            
        tui->num_options = tui->rooms_count;
        tui->option = 0;
        tui->loading = false;
        tui->draw_current = tui_draw_list;
        tui->draw_current(tui);
    }
}

void tui_get_join(tui_t* tui) {
    //tui jest w stanie chat i przestaje ładować, laczy sie z czatem
    tui_log(tui, "tutaj tui_get_join!");
    if(tui->mode == TUI_CHAT && tui->loading) {
        tui->num_options = 0;
        tui->option = 0;
        tui->loading = false;
        tui->draw_current = tui_draw_chat;
        tui->draw_current(tui);
    }
}

void tui_get_send(tui_t* tui) {
    tui_log(tui, "tutaj tui_get_send!");
    if(tui->mode == TUI_CHAT) {
        tui->input_buf[0] = '\0';
        tui->cursor = 0;
        tui->input_len = 0;
    }
}

void tui_get_registered(tui_t* tui) {
    tui_log(tui, "tutaj tui_get_registered!");
    //tui jest w chat i przestaje ładować, łączy z czatem ale tutaj to my jestesmy hostem bo z create
    if(tui->mode == TUI_CHAT && tui->loading) {
        tui->num_options = 0;
        tui->option = 0;
        tui->loading = false;
        tui->draw_current = tui_draw_chat;
        tui->draw_current(tui);
    }
}


void tui_draw_start_screen(tui_t *tui) {
    refresh();
    werase(tui->chat_win); box(tui->chat_win, 0, 0);
    wrefresh(tui->chat_win);
    mvwprintw(tui->chat_win, 2, 2, "WITAJ W P2P2");
    mvwprintw(tui->chat_win, 4, 2, "Aby kontynuowac, wpisz swoj nick ponizej.");
    mvwprintw(tui->chat_win, 5, 2, "Nastepnie nacisnij ENTER.");
    wrefresh(tui->chat_win);
    
    // Ustawiamy kursor w oknie inputu
    wmove(tui->input_win, 1, 1);
    mvwprintw(tui->input_win, 1, 1, "NICK: "); 
    wrefresh(tui->input_win);
}

void tui_draw_menu(tui_t *tui) {
    refresh();
    werase(tui->chat_win); box(tui->chat_win, 0, 0);
    wrefresh(tui->chat_win);

    const char* opcje[] = {"1. Dolacz do pokoju", "2. Hostuj pokoj", "3. Wyjdz"};

    mvwprintw(tui->chat_win, 2, 2, "Witaj '%s'!", tui->user_data.nick);
    for(int i = 0; i < 3; ++i) {
        if(tui->option == i) wattron(tui->chat_win, A_REVERSE | A_BOLD);
        mvwprintw(tui->chat_win, 3+i, 3, "%s", opcje[i]);
        if(tui->option == i) wattroff(tui->chat_win, A_REVERSE | A_BOLD);
    }
    wrefresh(tui->chat_win);
    
    // Ustawiamy kursor w oknie inputu
    wmove(tui->input_win, 1, 1);
    mvwprintw(tui->input_win, 1, 1, "NICK: "); 
    wrefresh(tui->input_win);
}

void tui_draw_list(tui_t *tui) {
    refresh();
    werase(tui->chat_win); box(tui->chat_win, 0, 0);
    wrefresh(tui->chat_win);

    mvwprintw(tui->chat_win, 2, 2, "Witaj '%s'!", tui->user_data.nick);
    if(tui->rooms_count == 0) {
        mvwprintw(tui->chat_win, 3, 3, "Niestety! Nikt nie hostuje pokoju. Popros kolege/kolezanke (jesli masz) o zahostowanie pokoju, albo zahostuj go sam!");
    } else {
        for(int i = 0; i < tui->rooms_count; ++i) {
            if(tui->option == i) wattron(tui->chat_win, A_REVERSE | A_BOLD);
            mvwprintw(tui->chat_win, 3+i, 3, "%s | %d", tui->rooms[i].name, tui->rooms[i].room_id);
            if(tui->option == i) wattroff(tui->chat_win, A_REVERSE | A_BOLD);
        }
    }
    wrefresh(tui->chat_win);
    
    // Ustawiamy kursor w oknie inputu
    wmove(tui->input_win, 1, 1);
    mvwprintw(tui->input_win, 1, 1, "WYBOR: "); 
    wrefresh(tui->input_win);
}

void tui_draw_create(tui_t *tui) {
    refresh();
    werase(tui->chat_win); box(tui->chat_win, 0, 0);
    wrefresh(tui->chat_win);

    mvwprintw(tui->chat_win, 2, 2, "Witaj '%s'!", tui->user_data.nick);
    mvwprintw(tui->chat_win, 3, 3, "Prosze podaj nazwe swojego pokoju");
    wrefresh(tui->chat_win);
    
    // Ustawiamy kursor w oknie inputu
    wmove(tui->input_win, 1, 1);
    mvwprintw(tui->input_win, 1, 1, "NAZWA: "); 
    wrefresh(tui->input_win);
}

//je suis khabat salope
void tui_draw_chat(tui_t *tui) {
    werase(tui->chat_win);
    box(tui->chat_win, 0, 0);

    int win_h, win_w;
    getmaxyx(tui->chat_win, win_h, win_w);
    
    int usable_w = win_w - 2;
    int bubble_w = (int)(usable_w * 0.7);
    int current_y = win_h - 2; 

    int total = tui->full ? MAX_HISTORY : tui->head;

    //przechodzi przez cala historie ale ostatecznie i 
    //tak wyswietli tyle wiadomosci ile sie miesci na ekranie
    for (int i = 0; i < total; i++) {
        int idx = (tui->head - 1 - i + MAX_HISTORY) % MAX_HISTORY;
        chat_entry_t *entry = &tui->history[idx];
        
        bool is_me = (strcmp(entry->nick, tui->user_data.nick) == 0);
        char display_msg[MESS_LEN + NICK_LEN + 10];
        
        if (entry->system) {
            snprintf(display_msg, sizeof(display_msg), "* %s", entry->msg);
        } else if (is_me) {
            snprintf(display_msg, sizeof(display_msg), "%s", entry->msg);
        } else {
            snprintf(display_msg, sizeof(display_msg), "<%s> %s", entry->nick, entry->msg);
        }

        //plan zawijania wiadomosci
        int target_w = entry->system ? usable_w : bubble_w;
        wrapped_info_t info = wrap_text(display_msg, target_w);

        //to nie pozwoli na wyswietlanie starych wiadomosci
        if (current_y - info.count + 1 < 1) break;

        int start_y = current_y - info.count + 1;

        //rysowanie kazdej linii
        for (int j = 0; j < info.count; j++) {
            int line_len = info.line_lengths[j];
            int line_off = info.line_offsets[j];
            
            int x = 1;
            if (is_me && !entry->system) {
                if(info.count > 1)
                    x = win_w - target_w - 1;
                else 
                    x = win_w - line_len - 1;
            }

            if (is_me) wattron(tui->chat_win, A_BOLD);
            mvwprintw(tui->chat_win, start_y + j, x, "%.*s", line_len, display_msg + line_off);
            if (is_me) wattroff(tui->chat_win, A_BOLD);
        }

        current_y = start_y - 1;
        if (current_y < 1) break;
    }

    wrefresh(tui->chat_win);

    wmove(tui->input_win, 1, tui->cursor + 3);
    wrefresh(tui->input_win);
}

void tui_draw_loading(tui_t *tui) {
    werase(tui->chat_win);
    char* msg = "Ladowanie...";
    box(tui->chat_win, 0, 0);
    mvwprintw(tui->chat_win, LINES/2 - 2, (COLS*0.618)/2 - strlen(msg)/2, "%s", msg);
    //animacja??? | / -- \ | / -- \ |
    wrefresh(tui->chat_win);
}

void tui_handle_mode(tui_t *tui) {
    switch(tui->mode) {
        case TUI_LISTING: {
            if (tui->rooms_count != -1) {
                tui->num_options = tui->rooms_count;
                tui->option = 0;
                tui->loading = false;
                tui->draw_current = tui_draw_list;
            }
        } break;
        case TUI_CREATE: break;
        case TUI_CHAT: break;
        case TUI_START: break;
        case TUI_EXIT: break;
        case TUI_MENU: break;
        default: break;
    }
    tui->draw_current(tui);
}

void tui_log(tui_t *tui, const char *format, ...) {
    va_list args;
    va_start(args, format);

    int h = getmaxy(tui->log_win); 
    wmove(tui->log_win, h - 2, 1);

    vw_printw(tui->log_win, format, args);
    wprintw(tui->log_win, "\n");

    box(tui->log_win, 0, 0);
    mvwprintw(tui->log_win, 0, 2, "[ LOGI ]");

    wrefresh(tui->log_win);
    va_end(args);
}

//dodaje wiadomosc nowa
void tui_add_mess(tui_t *tui, chat_entry_t entry) {
    if(tui->mode == TUI_CHAT) {
        tui->history[tui->head] = entry;

        //glowa o jeden dalej
        tui->head = (tui->head + 1) % MAX_HISTORY;

        //cyklizm lol
        if (tui->head == 0 && !tui->full) {
            tui->full = true;
        }

        tui->draw_current(tui);
    }
}