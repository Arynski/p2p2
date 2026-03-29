#ifndef TUI_H
#define TUI_H
#include <stdint.h>
#include <ncurses.h>
#include "../../common/protocol_mess.h"
#include "../../common/protocol_STUN.h"

typedef struct {
    void (*on_msg)(void *ctx, const char *nick, const char *mess);
    void (*on_join)(void *ctx, const char *nick);
    void (*on_leave)(void *ctx, const char *nick);
    void (*on_kick)(void *ctx);
    void (*on_frame)(void *ctx, const char *dir, uint8_t type);
} chat_callbacks_t;

typedef enum {
    TUI_START   =   0x01,
    TUI_MENU    =   0x02,
    TUI_LISTING =   0x03,
    TUI_CREATE  =   0x04,
    TUI_CHAT    =   0x05,
    TUI_EXIT    =   0x06,
} tui_mode_t;

typedef struct {
    char nick[NICK_LEN];
    int mode; //0 host, 1 peer
} user_t;

typedef struct {
    WINDOW *chat_win;
    WINDOW *log_win;
    WINDOW *input_win;
    //kiedy cos wpisuje
    char    input_buf[MESS_LEN];
    int     input_len;
    int     cursor;
    //kiedy cos wybiera
    int option;
    int num_options;
    
    chat_callbacks_t cb;
    tui_mode_t mode;
    user_t user_data;
} tui_t;

/*inicjalizuje trzy okna, ustawia odpowiednie ustawienia*/
void tui_init(tui_t* tui);

/*uzywa getch, pobiera jeden znak i robi to co musi (np backspace, delete, enter, znaki), zwraca 1 jesli enter
i 0 jesli nie enter lol*/
uint8_t tui_process_input(tui_t* tui); 

void tui_on_msg(tui_t* tui, const char *nick, const char *mess);
void tui_on_join(tui_t* tui, const char *nick);
void tui_on_leave(tui_t* tui, const char *nick);
void tui_on_kick(tui_t* tui);
void tui_on_frame(tui_t* tui, const char *dir, uint8_t type);
void tui_start_screen(tui_t *tui);
void tui_draw_menu(tui_t *tui);
void tui_handle_mode(tui_t *tui);

#endif