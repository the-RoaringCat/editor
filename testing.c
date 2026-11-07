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
    int cursor_col;
    int cursor_row;
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
    context.cursor_col = 1;
    context.cursor_row = 1;
    context.path = path;
}
char *keywords[] = {"for", "while", "if", "else"};
char *types[] = {"int", "bool", "char", "float"};

void render_a_line(STRINGBUILDER line) {
    STRINGBUILDER token = da_INIT(STRINGBUILDER);

    for (size_t i = 0; i < line.len; i++) {
        char c = line.items[i];
        if (ispunct(c) || isspace(c) || c == '\n') {
            if (token.len != 0) {
                for (size_t j = 0; j < sizeof(keywords)/sizeof(keywords[0]); j++) {
                    size_t len = strlen(keywords[j]);
                    if (token.len == len && memcmp(token.items, keywords[j], len) == 0) {
                        write(STDOUT_FILENO, "\x1b[31m", 5);  //red
                        goto print;
                    }
                }
                for (size_t j = 0; j < sizeof(types)/sizeof(types[0]); j++) {
                    size_t len = strlen(types[j]);
                    if (token.len == len && memcmp(token.items, types[j], len) == 0) {
                        write(STDOUT_FILENO, "\x1b[32m", 5);  //green
                        goto print;
                    }
                }
            print:
                write(STDOUT_FILENO, token.items, token.len);
                write(STDOUT_FILENO, "\x1b[0m", 4);   // Reset all styles
                token.len = 0;
            }
            write(STDOUT_FILENO, &c, 1);
        } else {
            da_append(token, c);
        }
        
    }   
}

void render() {
    update_term_size();
    set_global_cursor_coordinate(&context.cursor_col, &context.cursor_row);
    if (context.cursor_row < context.offset_row + 1) {
        context.offset_row--;
    }

    if (context.cursor_row > context.offset_row + context.size_row - 1) {
        context.offset_row++;
    }

    int checked_index;
    PIECE *current;

    char *piece_buffer;
    STRINGBUILDER line = da_INIT(STRINGBUILDER);
    int total_line_count = 0;
    int printed_line_count = 0; 

    for (current = sentinel_head->next; current->next; current = current->next) {
        for (int i = 0; i < current->len; i++) { 
            piece_buffer = current->type == ORIGINAL ? original.items : added.items;
            char c = piece_buffer[current->offset + i];

            if (printed_line_count == context.size_row - 1) 
                goto done;
            
            if (c == '\n') {
                total_line_count++;
                if (total_line_count > context.offset_row) {
                    printed_line_count++;
                    da_append(line, '\n');
                    render_a_line(line);
                    //write(STDOUT_FILENO, line.items, line.len);
                    line.len = 0;
                }
            } else {
                if (total_line_count >= context.offset_row)
                    da_append(line, c);
            } 
        }
    }
    
done:
    free(line.items);
    char buffer[100];
    int len;
    

    //bottom line info
    len = snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", context.size_row, 1);
    write(STDOUT_FILENO, buffer, len);  //move to the bottom line
    write(STDOUT_FILENO, "\x1b[44;37m", 8); //set blue background and white foreground
    write(STDOUT_FILENO, "\x1b[2K", 4); //fill entire row with background color
    len = snprintf(buffer, sizeof(buffer), "editor x: %d,y: %d, x_os: %d, y_os: %d, text_index: %d", context.cursor_row, context.cursor_col, context.offset_row, context.offset_col, text_index);
    write(STDOUT_FILENO, buffer, strlen(buffer)); //print cursor coordinate
    write(STDOUT_FILENO, "\x1b[0m", 4); //reset colors

    //set cursor to where u type

    len = snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", context.cursor_row - context.offset_row, context.cursor_col - context.offset_col);
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
        if (sb.len != 0) 
            sb.len = 0;
        
        p_move_left(); 
        break;

    case KEY_RIGHT:
        if (sb.len != 0) 
            sb.len = 0;
        
        p_move_right();
        break;
    case KEY_ESCAPE:
        if (sb.len != 0) 
            sb.len = 0;
        
        //undo();
        //save the file
        insert(sb); //in case the current editting field is not saved
        save_file(file_path);
        write(STDOUT_FILENO, "\x1b[?1049l", 8); //leave alternate screen
        exit(0);
        break;
   
    case KEY_BACKSPACE:
        if (sb.len != 0) {
            sb.len--;
        }
        delete();
        break;
    case KEY_UP:
        if (sb.len != 0)
            sb.len = 0;

        p_move_up(context.cursor_col);
        break;
    case KEY_DOWN:
        if (sb.len != 0)
            sb.len = 0;

        p_move_down(context.cursor_col);
        break;
    case KEY_NULL:
        break;
    case KEY_ENTER:
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
   
    write(STDOUT_FILENO, "\x1b[?1049h", 8);  //alternate screen 
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





