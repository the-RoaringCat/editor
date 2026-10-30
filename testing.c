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


void render() {
    PIECE *curr = sentinel_head->next;

    for (; curr->next; curr = curr->next) {
        STRINGBUILDER sb = (curr->type == ORIGINAL) ? original : added;
        write(STDOUT_FILENO, sb.items + curr->offset, curr->len); 
    }

    printf("===END===\n");
}

int main() {
    buffer_init("test.c");
    piece_table_init();
    
    text_index = 1;
    STRINGBUILDER sb = da_INIT(STRINGBUILDER);
    da_append(sb, 'X');
    da_append(sb, 'Z');
    da_append(sb, 'Y');
    insert(sb);

    text_index = 2;
    sb = da_INIT(STRINGBUILDER);
    da_append(sb, 'x');
    da_append(sb, 'z');
    da_append(sb, 'y');
    insert(sb);
    render();



    //free_piece_table();
}


