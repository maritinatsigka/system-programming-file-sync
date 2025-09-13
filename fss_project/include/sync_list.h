#ifndef SYNC_LIST_H
#define SYNC_LIST_H

#include <limits.h>
#include <stdio.h>
#include <time.h>

typedef struct sync_node{
    char src[PATH_MAX];
    char trg[PATH_MAX];
    char last_sync[64];
    int active;
    int syncing; 
    int errors;
    char result[32];
    struct sync_node *next;

}sync_node;

extern sync_node *sync_list;

int add_sync_pair(const char *src, const char *trg); //adds a new sync pair to the sync list 
sync_node *find_sync_pair(const char *src); //finds a sync pair by source directory path
void print_status(const char *src, int fd); //prints the status of a specific sync pair to a file descriptor 
void free_sync_list(); //frees all nodes from the sync list 
int start_manual_sync(const char *src, char *trg_out); //starts a manual sync for a specific source directory 
int cancel_sync_pair(const char *src); //cancels the monitoring of a specific source directory 

#endif