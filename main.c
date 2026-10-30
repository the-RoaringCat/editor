#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <termios.h>

#include "common.h"


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

void node_append()
char *orig_buf;
STRINGBUILDER added_buf;

/*---------
 The piece table is abstracted to a continuous array
 Then the array is abstracted to lines
*/
int text_ptr = 0;   //points to the character in the abstracted array
int page_ptr = 0;   //points to the start of the first line in the terminal's view
int total_len;

NODE *current_node;
int current_len;    //len of characters in the current piece and before the cursor
//terminal's dimension
unsigned short ws_row;   
unsigned short ws_col; 

//list of piecetable
LIST list;

Cursor cursor_position = {1, 1}; // 1 based since row and col are also 1 based

V2 find_global_cursor_coordinate() {
    int row = 1;
    int col = 1;
    int next_char_ptr = 0; //the next char to be checked against '\n'

    NODE *current = list.head;
    
    for (NODE *current = list.head; current != NULL; current = current->next) {
        PIECE piece = *(PIECE *)(current->item);
        for (int i = 0; i < piece.len; i++) { 
            if (next_char_ptr == text_ptr) goto done;   //we dont check the cursor's char

            char *buf = piece.type == ORIGINAL ? orig_buf : added_buf.items;
            if (buf[piece.offset + i] != '\n') {
                col++;
            } else {
                row++;
                col = 1;
            }
            next_char_ptr++;
        }
    }

    done:
        return (V2){col, row};
}

void find_current_node() {
    int len = 0;    //len of pieces before the piece that holds the cursor  
    for (NODE *current = list.head; current != NULL; current = current->next) {
        PIECE piece = *(PIECE *)(current->item);
        if (text_ptr < (len + piece.len)) {
            current_len = text_ptr - len;
            current_node = current;
            return;
        }
        len += piece.len;
    }
    current_node = current;
}

void update_cursor_position(KeyboardKey key) {
    //TODO rolling  
    //V2 global_cursor_coordinate = find_global_cursor_coordinate();
    switch (key) {
    case KEY_LEFT:
        if (text_ptr > 0) text_ptr--;
        
        break;
    case KEY_RIGHT:
        if (text_ptr < total_len) text_ptr++;
        break;
    }

}

void update_term_dimension() {
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    ws_row = ws.ws_row;
    ws_col = ws.ws_col;
}

KeyboardKey get_input() {
    char buf[4] = {'\0', '\0', '\0', '\0'};
    int nread = read(STDIN_FILENO, buf, 4);
    //printf("Key read: %d, %d, %d, %d\n", (int)buf[0], (int)buf[1], (int)buf[2], (int)buf[3]);
    if (buf[0] == '\b' || buf[0] == 127) {
        return KEY_BACKSPACE;
    }
    if (isprint(buf[0])) {
        return (KeyboardKey)buf[0];
    } 
    if (buf[0] == '\x1b') {
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

void move_cursor() {   
    cursor_position = find_global_cursor_coordinate(); 
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", cursor_position.row, cursor_position.col);
    write(STDOUT_FILENO, buf, len);
}

void insert(KeyboardKey key) {
    PIECE *piece = current_node->item;
    switch(piece->type) {
    case ORIGINAL:
        int remaining_len = piece->len - current_len;
        PIECE *added_piece = malloc(sizeof(PIECE));
        if (remaining_len != 0) {
            PIECE *split_orig_piece = malloc(sizeof(PIECE));
            
            added_piece.

        }

    case ADDED:
    }
    total_len++;
    update_cursor_position(KEY_RIGHT);
    move_cursor();
}

void process_input(KeyboardKey key) {
    switch(key) {
    case KEY_UP:
    case KEY_DOWN:
    case KEY_RIGHT:
    case KEY_LEFT:
        update_cursor_position(key);
        break;
    case KEY_ESCAPE:
    case KEY_NULL:
        return;
    case KEY_BACKSPACE:
        
        //TODO update the piece table
        break;
    default:
        //here should be printable
        //write(STDOUT_FILENO, (char *)&key, 1);
        insert(key)
        break;
    }
}


int main(int argc, char **argv) {
    if (argc < 2) {
        printf("expecting one argument <file>\n");
        return 69;
    }

    added_buf = da_INIT(STRINGBUILDER);
    PIECE original = load(argv[1]); //exit upon error
    total_len = original.len;
    printf("len: %d\n", total_len);
    list = list_init();
    list_append(&list, (void *)&original);
    
    enable_raw_mode();

    clear_screen();
    update_term_dimension();
    render(list);
    move_cursor();
    
    while (1) {
        //clear_screen();
        update_term_dimension();
    

        //render(list); //TODO only re render when line change?    
        KeyboardKey key = get_input();
        process_input(key);  
        move_cursor();
    }
    

    return 0;
}

void clear_screen() {
    write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7); //clear screen, then move to topleft
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
    STRINGBUILDER sb = da_INIT(STRINGBUILDER);
    NODE *current = list.head;
    int ptr = 0;
    int line_count = 0;

    //print the line from the page ptr
    while (current) {
        int i = 0;

        PIECE piece = *(PIECE *)current->item;
        char *buf = (piece.type == ORIGINAL) ? (orig_buf + piece.offset) : (added_buf.items + piece.offset);
        while (i < piece.len) {
            if (ptr < page_ptr) goto done; //dont print

            char c = buf[i];
            if (c == '\n') {
                da_append(sb, '\r');
                da_append(sb, '\n');
                write(STDOUT_FILENO, sb.items, sb.len);
                da_free(sb);
                sb = da_INIT(STRINGBUILDER);
                line_count++;
                if(line_count == (ws_row - 1)) break; //max row(line) reached
            } else {
                da_append(sb, c);
            }



            done:
                i++;
                ptr++;
        }

        current = current->next;
    }

    //fill the unused line
    for (;line_count < ws_row - 1; line_count++) {
        write(STDOUT_FILENO, "~\r\n", 3);
    }


    //bottom line info
    // Set blue background and white foreground
    write(STDOUT_FILENO, "\x1b[44;37m", 8);

    // Fill entire row with background color
    write(STDOUT_FILENO, "\x1b[2K", 4);

    // Print title
    char *title = "\uE001\uE002EDITOR";
    write(STDOUT_FILENO, title, strlen(title));

    // Reset colors
    write(STDOUT_FILENO, "\x1b[0m", 4);
}

