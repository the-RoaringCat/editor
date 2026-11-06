#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <termios.h>

#define PIECE_TABLE_IMPLEMENTATION
#include "piece_table.h"
#include "common.h"

typedef struct editor_context {
    int current_line;
    int size_col;
    int size_row;
    int offset_col;
    int offset_row;
    char *path;
} CONTEXT;


CONTEXT context;
struct termios orig_termios;
int local_index = 0;
STRINGBUILDER sb;
char *file_path;


void update_term_size() {
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    context.size_col = ws.ws_col;
    context.size_row = ws.ws_row;
}

void editor_init(char *path) {
    update_term_size();
    context.current_line = 0;
    context.offset_col = 0;
    context.offset_row = 0;
    context.path = path;
}

// void render1() {
//     PieceIterator iterator = {sentinel_head->next, sentinel_tail, 0};
//     PIECE *pp;
//     int checked_index;
//     int line_count = 0;
//     int local_offset;

//     //look for the first line to print
//     while (pp = piece_next(&iterator, &checked_index)) {
//         char *buf = pp->type == ORIGINAL ? original.items : added.items;
//         for (int i = 0; i < pp->len; i++) {
//             if (line_count == context.current_line) { //check condition in the next loop to exclude the '\n'
//                 local_offset = i;
//                 goto done_line;
//             }
//             if (buf[pp->offset + i] == '\n') {
//                 line_count++;
//             } 

//         }
//     }

// done_line:
//     line_count = 0;
//     //print from the current pp's offset until the line with cursor
//     do {
//         for ()
//     } while (pp = piece_next(&iterator, &checked_index))


// }

void render() {
    
    update_term_size();

    int checked_index;
    PIECE *current;
    PIECE *text_index_piece = find_piece_from_index(text_index, &checked_index);


    STRINGBUILDER buf;

    //for (current = sentinel_head->next; current != text_index_piece ; current = current->next){
    for (current = sentinel_head->next; current->next ; current = current->next){
        buf = (current->type == ORIGINAL) ? original : added;
        write(STDOUT_FILENO, buf.items + current->offset, current->len); 
    }

    // buf = (current->type == ORIGINAL) ? original : added;
    // int left_len = text_index - checked_index;
    // write(STDOUT_FILENO, buf.items + text_index_piece->offset, left_len);
    // write(STDOUT_FILENO, sb.items, sb.len);
    // write(STDOUT_FILENO, buf.items +text_index_piece->offset + left_len, text_index_piece->len - left_len);

    // for (current = current->next; current->next; current = current->next) {
    //     buf = (current->type == ORIGINAL) ? original : added;
    //     write(STDOUT_FILENO, buf.items + current->offset, current->len); 
    // }

    char buffer[40];
    int len;
    V2 cursor_position = find_global_cursor_coordinate();

    //bottom line info
    len = snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", context.size_row, 1);
    write(STDOUT_FILENO, buffer, len);  //move to the bottom line
    write(STDOUT_FILENO, "\x1b[44;37m", 8); //set blue background and white foreground
    write(STDOUT_FILENO, "\x1b[2K", 4); //fill entire row with background color
    len = snprintf(buffer, sizeof(buffer), "editor x: %d,y: %d, text_index: %d", cursor_position.row, cursor_position.col, text_index);
    write(STDOUT_FILENO, buffer, strlen(buffer)); //print cursor coordinate
    write(STDOUT_FILENO, "\x1b[0m", 4); //reset colors

    //set cursor to where u type
    len = snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", cursor_position.row, cursor_position.col);
    write(STDOUT_FILENO, buffer, len);   
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

KeyboardKey get_input() {
    char buf[4] = {'\0', '\0', '\0', '\0'};
    int nread = read(STDIN_FILENO, buf, 4);
    //printf("Key read: %d, %d, %d, %d\n", (int)buf[0], (int)buf[1], (int)buf[2], (int)buf[3]);
    if (buf[0] == '\r') {
        return KEY_ENTER;
    }
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

void save_file(char *path) {
    FILE *fp;
    fp = fopen(path, "wb");
    if (fp == NULL) {
        printf("fopen: cannot open file for saving\n");
        exit(67);
    }
    
    PIECE *current;
    for (current = sentinel_head->next; current->next; current= current->next) {
        char *buf = current->type == ORIGINAL ? original.items : added.items;
        fwrite(buf + current->offset, 1, current->len, fp);
    }
    fclose(fp);
}


void process_key(KeyboardKey key) {
    switch(key) {
    case KEY_LEFT:
        if (sb.len != 0) {
            //insert(sb);
            free(sb.items);
            sb = da_INIT(STRINGBUILDER);
        }
        p_move_left(); 
        break;

    case KEY_RIGHT:
        if (sb.len != 0) {
            //insert(sb);
            free(sb.items);
            sb = da_INIT(STRINGBUILDER);
        }
        p_move_right();
        break;
    case KEY_ESCAPE:
        if (sb.len != 0) {
            free(sb.items);
            sb = da_INIT(STRINGBUILDER);
        }
        undo();
        //save the file
        //insert(sb); //in case the current editting field is not saved
        //save_file(file_path);
        //write(STDOUT_FILENO, "\x1b[?1049l", 8); //leave alternate screen
        //exit(0);
        break;
    case KEY_ENTER:
        da_append(sb, '\n');
        //insert(sb);
        sb = da_INIT(STRINGBUILDER);
        break;
    case KEY_BACKSPACE:
        delete();
        break;
    case KEY_UP:
    case KEY_DOWN:
    case KEY_NULL:
        break;
    default:
        if (sb.len != 0) {
            undo();
        }
        da_append(sb, (char)key);
        insert(sb);
        break;
    }
}


int main(int argc, char **argv) {
    if (argc < 2) {
        printf("expecting one argument <file>\n");
        return 69;
    }
    file_path = argv[1];
    buffer_init(file_path);
    piece_table_init();
    editor_init(argv[1]);
    
    enable_raw_mode();
   
    //write(STDOUT_FILENO, "\x1b[?1049h", 8);  //alternate screen 
    sb = da_INIT(STRINGBUILDER);

    clear_screen();
    render();
    write(STDOUT_FILENO, "\x1b[H", 3);
    while (1) {
        KeyboardKey key = get_input();
        process_key(key);
        clear_screen();
        render();
    }

    
    //free_piece_table();
}





