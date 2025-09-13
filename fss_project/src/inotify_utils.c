#include "../include/inotify_utils.h"
#include "../include/sync_list.h"
#include "../include/manager_utils.h"
#include <sys/inotify.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#define MAX_FILE_PATH (PATH_MAX + NAME_MAX + 2)

int inotify_fd = -1;
int watch_count = 0;
watch_entry watch_table[MAX_PAIRS];


void init_inotify(){
    inotify_fd = inotify_init1(IN_NONBLOCK);
    if(inotify_fd < 0){
        perror("inotify_init");
        exit(1);
    }
}


//add a directory to the inotify watch list 
void add_watch(const char *src){

    if(watch_count >= MAX_PAIRS){
        fprintf(stderr, "Watch limit reached.\n");
        return;
    }

    int wd = inotify_add_watch(inotify_fd, src, IN_CREATE | IN_MODIFY | IN_DELETE);
    if(wd == -1){
        fprintf(stderr, "Could not watch %s\n", src);
        return;
    }

    log_and_print("[WATCH] Adding watch to: %s", src);
    watch_table[watch_count].wd = wd;
    strncpy(watch_table[watch_count].src, src, PATH_MAX);
    watch_count++;
}


//handle inotify events and trigger workers accordingly 
void handle_inotify_events(){
    char buffer[EVENT_BUF_LEN];
    int length = read(inotify_fd, buffer, EVENT_BUF_LEN);

    if(length <= 0){
        if(errno != EAGAIN){
            perror("read");
        }
        return;
    }

    int i = 0;
    while(i < length){
        struct inotify_event *event = (struct inotify_event *)&buffer[i];

        if(event->len > 0){
            const char *type = NULL;
            
            //identify event type 
            if(event->mask & IN_CREATE){
                type = "ADDED";
            }
            if(event->mask & IN_MODIFY){
                type = "MODIFIED";
            }
            if(event->mask & IN_DELETE){
                type = "DELETED";
            }

            if(type){

                log_and_print("[INOTIFY] Event detected: %s (%s)", event->name, type);

                //find corresponding source directory 
                for(int j = 0; j < watch_count; j++){
                    if(watch_table[j].wd == event->wd && watch_table[j].src[0] != '\0'){
                        sync_node *entry = find_sync_pair(watch_table[j].src);
                        if(entry && entry->active){

                            //create a pipe for communication 
                            int pipe_fd[2];
                            if (pipe(pipe_fd) == -1) {
                                perror("pipe failed");
                                continue;
                            }

                            pid_t pid = fork();
                            if(pid == 0){
                                //child process: redirect output and execute worker 
                                close(pipe_fd[0]); 
                                dup2(pipe_fd[1], STDOUT_FILENO);
                                close(pipe_fd[1]);
                                execl("bin/worker", "worker", entry->src, entry->trg, event->name, type, NULL);
                                perror("execl failed");
                                exit(1);
                            } else if(pid > 0){
                                //parent process: read report from worker 
                                close(pipe_fd[1]);

                                char report_buf[1024];
                                ssize_t n = read(pipe_fd[0], report_buf, sizeof(report_buf) - 1);

                                report_buf[n] = '\0';
                                close(pipe_fd[0]);

                                //parse the EXEC_REPORT output 
                                char *status = strstr(report_buf, "STATUS:");
                                char *details = strstr(report_buf, "DETAILS:");
                                if(status){
                                    status += strlen("STATUS:");
                                }
                                if(details){
                                    details += strlen("DETAILS:");
                                }

                                char status_clean[32];
                                char details_clean[1024];
                                sscanf(status, "%31s", status_clean);
                                sscanf(details, "%1023[^\n]", details_clean);

                                //log the result 
                                log_worker_report(entry->src, entry->trg, event->name, type, status_clean, details_clean, pid);
                            } else{
                                perror("fork failed");
                            }
                        }
                    }
                }
            }
            int event_size = sizeof(struct inotify_event) + event->len;
            i += event_size;
        }
    }
}
