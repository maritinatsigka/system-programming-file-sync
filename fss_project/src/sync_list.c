#include "../include/sync_list.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>


sync_node *sync_list = NULL;

//add a new (source, target) synchronization pair 
int add_sync_pair(const char *src, const char *trg){

    sync_node *curr = sync_list;

    //check if the source already exists 
    while(curr){
        if(strcmp(curr->src, src) == 0){
            return 0;  //already exists
        }
        curr = curr->next;
    }

    sync_node *new_pair = malloc(sizeof(sync_node));
    if(!new_pair){
        return -1;
    }

    strncpy(new_pair->src, src, PATH_MAX);
    strncpy(new_pair->trg, trg, PATH_MAX);
    new_pair->active = 1;
    new_pair->errors = 0;
    new_pair->syncing = 0;

    strcpy(new_pair->result, "PENDING");

    //set current timestamp at last sync 
    time_t now = time(NULL);
    strftime(new_pair->last_sync, sizeof(new_pair->last_sync), "%F %T", localtime(&now));
    new_pair->next = sync_list;
    sync_list = new_pair;

    return 1;
}


//find a sync pair by its source directory 
sync_node *find_sync_pair(const char *src){
    sync_node *curr = sync_list;
    while(curr){
        if(strcmp(curr->src, src) == 0){
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}


//cancel monitoring for a given source directory 
int cancel_sync_pair(const char *src){
    sync_node *entry = find_sync_pair(src);
    if(entry && entry->active){
        entry->active = 0;
        return 1;
    }
    return 0;
}


//print the status information of a monitored directory 
void print_status(const char *src, int fd){
    sync_node *entry = find_sync_pair(src);
    if(!entry){
        dprintf(fd, "Directory not monitored: %s\n", src);
        return;
    }

    const char *status = entry->active ? "Active" : "Inactive";
    dprintf(fd, "Directory: %s\nTarget: %s\nLast Sync: %s\nErrors: %d\nStatus: %s\n", entry->src, entry->trg, entry->last_sync, entry->errors, status);
}


//free the entire synchronization list from memory 
void free_sync_list(){
    sync_node *curr = sync_list;
    while(curr){
        sync_node *tmp = curr;
        curr = curr->next;
        free(tmp);
    }
    sync_list = NULL;
}


//mark a sync pair for manual synchronization 
int start_manual_sync(const char *src, char *trg_out){
    sync_node *entry = find_sync_pair(src);
    if(!entry || !entry->active){
        return 0; 
    }

    if(entry->syncing){
        return -1;  //already syncing 
    }

    entry->syncing = 1;
    strncpy(trg_out, entry->trg, PATH_MAX);
    return 1;
}