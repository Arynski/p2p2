#include "klient/tui/tui.h"
#include <ncurses.h>
#include "../../common/protocol_mess.h"
#include "../../common/protocol_STUN.h"

void tui_init(tui_t* tui) {
    setenv("ESCDELAY", "25", 1);
    initscr();
    cbreak();
    noecho();
    int height = LINES;
    int width = COLS;

    /*Czat po lewej, na dole pole do wpisywania, z prawej logi (1.618)*/
    int chat_x = (float)COLS*0.618; int chat_y = LINES-4;
    int input_x = chat_x; int input_y = 4;
    int log_x = COLS-chat_x; int log_y = LINES;

    *tui = (tui_t){
        .chat_win = newwin(chat_y, chat_x, 0, 0),
        .log_win = newwin(log_y, log_x, 0, chat_x),
        .input_win = newwin(input_y, input_x, chat_y, 0),
        .input_buf = "",
        .input_len = 0,
        .cursor = 0,
        .option = 0,
        .num_options = 0,
        .mode = TUI_START, 
        .user_data = (user_t){
            .mode = 0,
        },
    };

    box(tui->chat_win, 0, 0); box(tui->log_win, 0, 0); box(tui->input_win, 0, 0);

    keypad(tui->input_win, TRUE);
    refresh();
    wrefresh(tui->chat_win); wrefresh(tui->log_win); wrefresh(tui->input_win);
    nodelay(tui->input_win, 1);
    tui_draw_start_screen(tui);
}

uint8_t tui_process_input(tui_t* tui) {
    int in = wgetch(tui->input_win);
    switch(in) {
        case ERR: break;
        case KEY_ENTER: 
        case '\n': { 
            switch(tui->mode) {
                case TUI_START: {
                    strncpy(tui->user_data.nick, tui->input_buf, NICK_LEN - 1);
                    tui->user_data.nick[NICK_LEN - 1] = '\0';
                    tui->mode = TUI_MENU;
                    memset(tui->input_buf, 0, MESS_LEN);
                    tui->input_len = 0;
                    tui->cursor = 0;
                    tui->num_options = 3;
                } break;
                case TUI_MENU: {
                    switch(tui->option) {
                        case 0: tui->user_data.mode = 0; tui->mode = TUI_LISTING; break;
                        case 1: tui->user_data.mode = 1; tui->mode = TUI_CREATE; break;
                        case 2: tui->mode = TUI_EXIT; break;
                    }
                } break;
                case TUI_LISTING: break;
                case TUI_CREATE: break;
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
                tui_draw_menu(tui);
            }
        } break;
        case KEY_UP: {
            if(tui->option > 0) {
                tui->option--;
                tui_draw_menu(tui);
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
    
    // 2. Wypisujemy tekst (uważając na ramkę z prawej strony)
    mvwprintw(tui->input_win, 1, 1, "> %s", tui->input_buf);
    
    // 3. Odtwarzamy ramkę, którą wclrtoeol mógł uszkodzić
    box(tui->input_win, 0, 0);
    
    // 4. USTAWIDZIAMY KURSOR tam, gdzie logicznie powinien być
    // +1 za ramkę, +2 za "> "
    wmove(tui->input_win, 1, tui->cursor + 3);
    
    wrefresh(tui->input_win);

    return false;
}

void tui_on_msg(tui_t* tui, const char *nick, const char *mess) {

}

void tui_on_join(tui_t* tui, const char *nick) {

}

void tui_on_leave(tui_t* tui, const char *nick) {

}

void tui_on_kick(tui_t* tui) {

}

void tui_on_frame(tui_t* tui, const char *dir, uint8_t type) {
    
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

void tui_handle_mode(tui_t *tui) {
    switch(tui->mode) {
        case TUI_START: tui_draw_start_screen(tui); break;
        case TUI_MENU: tui_draw_menu(tui); break;
        case TUI_LISTING: break;
        case TUI_CREATE: break;
        case TUI_CHAT: break;
    }
}