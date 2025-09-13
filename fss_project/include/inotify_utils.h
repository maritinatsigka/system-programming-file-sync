#ifndef INOTIFY_UTILS_H
#define INOTIFY_UTILS_H

#include <limits.h>
#include "fss_manager.h"

#define EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + NAME_MAX + 1))


typedef struct{
    int wd;
    char src[PATH_MAX];
}watch_entry;


extern int inotify_fd;
extern watch_entry watch_table[MAX_PAIRS]; //all active watches 
extern int watch_count; //number of active watches


void init_inotify(); //initializes the inotify instance 
void add_watch(const char *src); //adds a new directroy to be watched 
void handle_inotify_events(); //handles inotify events and triggers appropriate synchronization



#endif