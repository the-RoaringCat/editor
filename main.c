#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <termios.h>

#include "common.h"

#define ORIGINAL 0
#define ADDED 1
#define FAIL 2

#define da_INIT(type) ((type){0, 0, NULL})
#define INIT_CAPACITY 1024

#define da_append(da, item)\
    do {\
        if (da.len == da.capacity) {\
            if (da.capacity == 0) {\
                da.capacity = INIT_CAPACITY;\
            } else {\
                da.capacity *= 2;\
            }\
            da.items = realloc(da.items, da.capacity * sizeof(*da.items));\
        }\
        da.items[da.len++] = item;\
    } while (0)

// #define ll_append(ll, item)
//     do {
//         NODE *new = malloc(sizeof(NODE));
//         new.item = 
//     } while (0)

typedef struct da_sb {
    size_t len;
    size_t capacity;
    char *items;
} STRINGBUILDER;

typedef struct piece {
    int type;
    size_t offset;
    size_t len;
} PIECE;

typedef struct NODE NODE;

typedef struct NODE {
    void *item;
    NODE *previous;
    NODE *next;
} NODE;

typedef struct ll {
    size_t len;
    NODE *head;
    NODE *tail;
} LIST;

typedef struct vector2d {
    int row;
    int col;
} V2;

typedef V2 Cursor;

struct termios orig_termios;

void clear_screen();
void disable_raw_mode();
void enable_raw_mode() ;

/*=====TODO=======

    1. I decide to implement a piece table, which could later be improved to a tree structure.
    2. Find a way to take input from a file
    3. A renderer to print the text onto the screen that support scrolling, editor meta data at the bottom, resizing etc
        This probably takes a long time.

     ======Example=====
     orig: hello world
    
     step: insert "hi "" in btw, and delete 'l' from word

     new: hello hi word
                type    | offset    | length
     pieces:    {original    0          6} => "hello "
                {added       0          3} => "hi "
                {original    6          3} => "wor"
                {original    10         1} => "d"
*/


PIECE load(char *path);
void render(LIST list);

LIST list_init() {
    LIST list = {0, NULL, NULL};
    return list;
}
void list_append(LIST *list, void *item) {
    NODE *new = malloc(sizeof(NODE));

    //init the new node
    new->item = item;
    new->previous = list->tail; //still work at 0 len, ie pointing to NULL
    new->next = NULL;
    
    if (list->len == 0) {
        list->head = new;
    } else {
        list->tail->next = new;
    }
   
    //update the tail
    list->tail = new;
    list->len++;
}
char *orig_buf;
char *added_buf = "hi ";

unsigned short ws_row;
unsigned short ws_col; 

Cursor cursor_position = {1, 1}; // 1 based since row and col are also 1 based

void update_term_dimension() {
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    ws_row = ws.ws_row;
    ws_col = ws.ws_col;
}

KeyboardKey get_input() {
    char buf[4] = {'\0', '\0', '\0', '\0'};
    int nread = read(STDIN_FILENO, buf, 4);

    if (isprint(buf[0])) {
        return (KeyboardKey)buf[0];
    } else if (buf[0] == '\x1b') {
        if (buf[1] == '[') {
            switch(buf[2]) {
            case 'A':
                return KEY_UP;
                break;
            case 'B':
                return KEY_DOWN;
                break;
            case 'C':
                return KEY_RIGHT;
                break;
            case 'D':
                return KEY_LEFT;
                break;
            default:
                return KEY_NULL;
                break;
            }
        } else {
            return KEY_ESCAPE;
        }
    } 

    return KEY_NULL; //anything on the keyboard not printable nor arrow
}

void move_cursor(KeyboardKey key) {
    switch (key) {
    case KEY_UP:
        write(STDOUT_FILENO, "\x1b[A", 3);
        break;
    case KEY_DOWN:
        write(STDOUT_FILENO, "\x1b[B", 3);
        break;
    case KEY_RIGHT:
        write(STDOUT_FILENO, "\x1b[C", 3);
        break;
    case KEY_LEFT:
        write(STDOUT_FILENO, "\x1b[D", 3);
        break;
    }
}

void process_input(KeyboardKey key) {
    switch(key) {
    case KEY_UP:
    case KEY_DOWN:
    case KEY_RIGHT:
    case KEY_LEFT:
        move_cursor(key);
        break;
    case KEY_ESCAPE:
    case KEY_NULL:
        return;
    default:
        write(STDOUT_FILENO, (char *)&key, 1);
        break;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("expecting one argument <file>\n");
        return 69;
    }

    //load original file
    
    PIECE original = load(argv[1]); //exit upon error
    PIECE added = {ADDED, 0, 3};
    
    enable_raw_mode();

    LIST list = list_init();
    list_append(&list, (void *)&original);
    list_append(&list, (void *)&added);
    //fprintf(stdout, "helloworld\x1b\n");

    clear_screen();
    while (1) {
        
        update_term_dimension();
        KeyboardKey key = get_input();
        process_input(key);  
    }
    

    return 0;
}

void clear_screen() {
    write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
}

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);//CR == NewLine,no flow control
    raw.c_oflag |= (OPOST | ONLCR); // \n becomes \r\n
    raw.c_cflag |= (CS8); //each character is 8bits
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN );
    //raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); //no echo, no canonical mode, no control char, no signal

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

PIECE load(char *path) {
    FILE *fp;
    STRINGBUILDER sb = da_INIT(STRINGBUILDER);
    
    fp = fopen(path, "r");
    if (fp == NULL) {
        printf("fopen: cannot open file\n");
        exit(67);
    }
    //read into original buffer
    while (1) {
        if (sb.len == sb.capacity) {
            //grow the array if needed, this also init the array to have non zero capacity
            da_append(sb, '\0');
            sb.len--;
        }
        size_t nread = fread(sb.items + sb.len, 1, sb.capacity - sb.len, fp);

        sb.len += nread;

        if (nread == 0) {
            if (ferror(fp)) {
                printf("fread: error in reading file\n");
                exit(69);
            }
            break;
        }
    }  
    
    PIECE piece = {ORIGINAL, 0, sb.len};
    orig_buf = sb.items;
    return piece;
}

void render(LIST list) {
    NODE *current = list.head;
    while (current) {
        PIECE piece = *(PIECE *)current->item;
        char *start_ptr;
        if (piece.type == ORIGINAL) {
            start_ptr = orig_buf + piece.offset;
        } else {
            start_ptr = added_buf + piece.offset;
        }
        write(STDOUT_FILENO, start_ptr, piece.len);

        current = current->next;
    }
    printf("END\n");
    return;
}

