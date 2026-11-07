#ifndef PIECE_TABLE_H
#define PIECE_TABLE_H

#include <stdio.h>
#include <assert.h>
#include "common.h"

//piece type
#define ORIGINAL    0
#define ADDED       1

int text_index;   //points to the combined resultant array
int total_len;  //len of the combind resultant array

typedef struct piece_descriptor {
    int type;
    size_t offset;
    size_t len;

    struct piece_descriptor *previous; //linked list
    struct piece_descriptor *next;
} PIECE;

typedef struct piece_iterator {
    PIECE *next;
    PIECE *end;
    int offset;
} PieceIterator;

typedef struct history {
    PIECE *left;
    PIECE *right;

    int len;
    int index;
    int is_boundry;
} HISTORY;

typedef struct da_history_stack {
    union {
        size_t len;
        size_t undo_len;
    };
    size_t capacity;
    size_t redo_len;
    HISTORY *items;
} HistoryStack;


void buffer_init(char *path);
void piece_table_init();
void meta_init();
PIECE *find_piece_from_index(int index, int *checked_index);
void insert();
void free_piece_table();
int p_move_left();
int p_move_right();

STRINGBUILDER original;
STRINGBUILDER added;
PIECE *sentinel_head;
PIECE *sentinel_tail;
HistoryStack history_stack;

#if defined(PIECE_TABLE_IMPLEMENTATION)

PIECE *piece_next(PieceIterator *iterator, int *start_index) {
    if (iterator->next == iterator->end) {
        return NULL;
    }

    PIECE *pp = iterator->next;         //the one inside iterator, to be returned
    *start_index = iterator->offset;    //document offset of the piece returned

    iterator->offset += pp->len;
    iterator->next = pp->next;
    
    return pp;
}

PIECE *find_piece_from_index(int index, int *checked_index) {
    //find the piece to insert
    PIECE *curr;
    int checked_len = 0;

    for(curr = sentinel_head->next; curr->next; curr = curr->next) {
        if (index < checked_len + curr->len) {
            if (checked_index != NULL) {
                *checked_index = checked_len;
            } 
            return curr;
        }

        checked_len += curr->len;
    }

    //curr point to tail
    //only when the index == checked len == total_len
    if (checked_index != NULL) {
        *checked_index = checked_len;
    }
    return curr;
}

int p_move_left() {
    if (text_index > 0) {
        text_index--;
        return 1;
    }
    return 0;
}

int p_move_right() {
    if (text_index < total_len - 1) {
        text_index++; 
        return 1;
    }
    return 0;
}

int p_move_up(int target_col) {
    if (text_index == 0) return 0;

    int checked_index;
    PIECE *current = find_piece_from_index(text_index, &checked_index);

    int is_previous_line = 0;
    int line_offset = 0;
    int line_len = 0;
    int i = text_index - checked_index - 1; //IGNORE the current one bcos the \n of the previous line 
                                            //mark the start of the current line

    for (; current->previous; current = current->previous) {
        for (; i >= 0; i--) {
            char *piece_buffer = current->type == ORIGINAL ? original.items : added.items;
            char c = piece_buffer[current->offset + i];

            if (!is_previous_line) {
                if (c == '\n') {
                    is_previous_line = 1;
                }
                line_offset++;
            } else {
                if (c == '\n') 
                    goto found;

                line_len++;

                if (text_index - line_offset - line_len == 0) //reach the first line
                    goto found;
            }
        }

        i = (int)current->previous->len - 1; //subsequent piece start from the end
    }
    

    return 0;

found:
    text_index -= line_offset + line_len;
    text_index += line_len < target_col ? line_len : (target_col - 1);
    
    return 1;
}

int p_move_down(int target_col) {
    
    int checked_index;
    PIECE *current = find_piece_from_index(text_index, &checked_index);

    int is_next_line = 0;
    int line_offset = 0;
    int line_len = 0;
    int i = text_index - checked_index; //include the current one

    for (; current->next; current = current->next) {
        for (; i < current->len; i++) { 
            char *piece_buffer = current->type == ORIGINAL ? original.items : added.items;
            char c = piece_buffer[current->offset + i];

            if (is_next_line == 0) {
                if (c == '\n') {
                    is_next_line = 1;      
                }
                line_offset++; 
            } else {
                line_len++;
                if (c == '\n') {
                    text_index += (line_offset - 1); //minus one because text_index's char is also checked 
                    text_index += (line_len < target_col ? line_len : target_col);
                    return 1; 
                } 
                              
            }                
        }
        i = 0; //subsequent pieces check from the start
    }

    return 0;    
}
void buffer_init(char *path) {
    added = da_INIT(STRINGBUILDER);
    original = da_INIT(STRINGBUILDER);

    //open the file
    FILE *fp;
    fp = fopen(path, "r");
    if (fp == NULL) {
        printf("fopen: cannot open file\n");
        exit(67);
    }

    //read into original buffer
    while (1) {
        if (original.len == original.capacity) {
            //grow the array if needed, this also init the array to have non zero capacity
            da_append(original, '\0');
            original.len--;
        }
        size_t nread = fread(original.items + original.len, 1, original.capacity - original.len, fp);

        original.len += nread;

        if (nread == 0) { //return value does not differentiate EOF and error
            if (ferror(fp)) {
                printf("fread: error in reading file\n");
                exit(69);
            }
            break;
        }
    } 
    fclose(fp);
}


void piece_table_init() {

    sentinel_head = malloc(sizeof(PIECE));
    sentinel_tail = malloc(sizeof(PIECE));
    PIECE *original_pd = malloc(sizeof(PIECE));

    *original_pd = (PIECE){ORIGINAL, 0, original.len, sentinel_head, sentinel_tail};

    *sentinel_head = (PIECE){0, 0, 0, NULL, original_pd};
    *sentinel_tail = (PIECE){0, 0, 0, original_pd, NULL};
    meta_init();
}

void meta_init() {
    text_index = 0;
    total_len = original.len;

    history_stack = da_INIT(HistoryStack);
}

void push_history(PIECE *left, PIECE *right, int is_boundry) {
    //pass by value to copy the original next and previous piece
    HISTORY history = {left, right, total_len, text_index, is_boundry};
    da_append(history_stack, history);
}

int undo() {
    if (history_stack.undo_len == 0) {
        return 0;
    }

    HISTORY history = history_stack.items[history_stack.undo_len - 1]; 

    //TODO redo stack got mem leak
    if (history.is_boundry) {
        history.left->next = history.right;
        history.right->previous = history.left;
    } else {
        history.left->previous->next = history.left;
        history.right->next->previous = history.right;
    }
  
    text_index = history.index;
    total_len = history.len;

    history_stack.undo_len--;
    history_stack.redo_len++;

    return 1;
}



static void added_append(STRINGBUILDER newtext) {
    for (size_t i = 0; i < newtext.len; i++) {
        da_append(added, newtext.items[i]);
    }
    //free(newtext.items);
}

void insert(STRINGBUILDER newtext) {
    if (newtext.len == 0) {
        return;
    }
    int checked_len;
    PIECE *old_p = find_piece_from_index(text_index, &checked_len);

    //added piece and store to added buffer
    PIECE *new_p = malloc(sizeof(PIECE));
    *new_p = (PIECE){ADDED, added.len, newtext.len, NULL, NULL}; //added's len is the offset
    added_append(newtext);
    

    //TODO now the left and right carry different meaning when we split the boundry and split the 
    //middle of a piece, maybe it can be fixed for readablility
    PIECE *left; 
    PIECE *right;

    if (text_index == checked_len) { //split at boundry
        left = old_p->previous;
        right = old_p; 
        push_history(left, right, 1);
    } else { //split at middle of piece
        push_history(old_p, old_p, 0);

        int left_len = text_index - checked_len;
        int right_len = old_p->len - left_len;

        left = malloc(sizeof(PIECE));
        right = malloc(sizeof(PIECE));

        old_p->previous->next = left;
        old_p->next->previous = right;

        *left = (PIECE){old_p->type, old_p->offset, left_len, old_p->previous, NULL};
        *right = (PIECE){old_p->type, old_p->offset + left_len, right_len, NULL, old_p->next};
        
        //free(old_p);
    }
       
    left->next = new_p;
    right->previous = new_p;

    new_p->previous = left;
    new_p->next = right;

    text_index+= newtext.len;
    total_len += newtext.len;
}

void delete() {
    if (total_len == 0 || text_index == 0) {
        return;
    }
    int checked_index;
    PIECE *pp = find_piece_from_index(text_index, &checked_index);
    
    int left_len = text_index - checked_index;
    if (left_len == 0) {
        //deleteing the end of previous piece
        if (pp->previous->len == 1) {
            pp->previous = pp->previous->previous;
            pp->previous->next = pp;
        } else {
            pp->previous->len--;
        }
        
    } else { 
        //deleting the middle of a piece
        PIECE *left_piece = malloc(sizeof(PIECE));  
        *left_piece = (PIECE){pp->type, pp->offset, left_len - 1, pp->previous, pp};
        pp->previous->next = left_piece;

        pp->offset += left_len;
        pp->len -= left_len;
        pp->previous = left_piece; 
    }
    

    text_index--;
    total_len--;
}


void set_global_cursor_coordinate(int *cursor_col, int *cursor_row) {
    int row = 1;
    int col = 1;
    int next_char_ptr = 0; //the next char to be checked against '\n'

    PIECE *current;
    
    for (current = sentinel_head->next; current->next; current = current->next) {
        for (int i = 0; i < current->len; i++) { 
            if (next_char_ptr == text_index) 
                goto done;   //we dont check the cursor's char

            char *buf = current->type == ORIGINAL ? original.items : added.items;
            if (buf[current->offset + i] != '\n') {
                col++;
            } else {
                row++;
                col = 1;
            }
            next_char_ptr++;
        }
    }

    done:
        *cursor_col = col;
        *cursor_row = row;
}

int find_next_line(int index) {
    int checked_index;
    int current_index;
    PIECE *current_piece = find_piece_from_index(text_index, &checked_index);

    for (; current_piece->next; current_piece = current_piece->next) {
        for (int i = 0; i < current_piece->len; i++) {
            STRINGBUILDER sb = current_piece->type == ORIGINAL ? original : added;
            current_index = checked_index + i;
            if ((current_index >= text_index) && (sb.items[current_piece->offset + i] == '\n')) {
                current_index += 1;
                return (current_index < total_len ? current_index : -1);
            }
        }
        checked_index += current_piece->len;
    }
    
    return -1; //no next line
}

void free_piece_table() {
    PIECE *curr = sentinel_head;

    while (curr) {
        PIECE *next = curr->next;
        free(curr);
        curr = next;
    }

    free(original.items);
    free(added.items);
}

#endif //PIECE_TABLE_IMPLEMENTATION
#endif //PIECE_TABLE_H