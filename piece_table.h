#ifndef PIECE_TABLE_H
#define PIECE_TABLE_H

#include <stdio.h>
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

void buffer_init(char *path);
void piece_table_init();
void meta_init();
PIECE *find_piece_from_index(int index, int *checked_index);
void insert();
void free_piece_table();
void p_move_left();
void p_move_right();

STRINGBUILDER original;
STRINGBUILDER added;
PIECE *sentinel_head;
PIECE *sentinel_tail;

#if defined(PIECE_TABLE_IMPLEMENTATION)

void p_move_left() {
    if (text_index > 0) {
        text_index--;
        write(STDOUT_FILENO, "\x1b[D", 3);
    }
}

void p_move_right() {
    if (text_index < total_len - 1) {
        text_index++;
        write(STDOUT_FILENO, "\x1b[C", 3);
    }
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
}

PIECE *find_piece_from_index(int index, int *checked_index) {
    //find the piece to insert
    PIECE *curr;
    int checked_len = 0;

    for(curr = sentinel_head->next; curr->next; curr = curr->next) {
        if (index < checked_len + curr->len) { 
            *checked_index = checked_len;
            return curr;
        }

        checked_len += curr->len;
    }

    //curr point to tail
    //only when the index == checked len == total_len
    *checked_index = checked_len;
    return curr;
}

static void added_append(STRINGBUILDER newtext) {
    for (size_t i = 0; i < newtext.len; i++) {
        da_append(added, newtext.items[i]);
    }
    free(newtext.items);
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
    total_len += newtext.len;

    //TODO now the left and right carry different meaning when we split the boundry and split the 
    //middle of a piece, maybe it can be fixed for readablility
    PIECE *left; 
    PIECE *right;

    if (text_index == checked_len) { //split at boundry
        left = old_p->previous;
        right = old_p; 
    } else { //split at middle of piece
        int left_len = text_index - checked_len;
        int right_len = old_p->len - left_len;

        left = malloc(sizeof(PIECE));
        right = malloc(sizeof(PIECE));

        old_p->previous->next = left;
        old_p->next->previous = right;

        *left = (PIECE){old_p->type, old_p->offset, left_len, old_p->previous, NULL};
        *right = (PIECE){old_p->type, old_p->offset + left_len, right_len, NULL, old_p->next};

        
        free(old_p);
    }
       
    left->next = new_p;
    right->previous = new_p;

    new_p->previous = left;
    new_p->next = right;

    text_index+= newtext.len;
}

V2 find_global_cursor_coordinate() {
    int row = 1;
    int col = 1;
    int next_char_ptr = 0; //the next char to be checked against '\n'

    PIECE *current;
    
    for (current = sentinel_head->next; current->next; current = current->next) {
        for (int i = 0; i < current->len; i++) { 
            if (next_char_ptr == text_index) goto done;   //we dont check the cursor's char

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
        return (V2){col, row};
}

PIECE *find_current_piece() {
    int len = 0;    //len of pieces before the piece that holds the cursor  
    PIECE *current;
    for (current = sentinel_head->next; current->next; current = current->next) {
        if (text_index < (len + current->len)) {
            return current;
        }
        len += current->len;
    }
    return current;
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