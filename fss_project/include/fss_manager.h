#ifndef FSS_MANAGER_H
#define FSS_MANAGER_H 

#include <stdio.h>
#include <limits.h>
#include <signal.h>

#define MAX_PAIRS 100  //maximum number of source-target diretcory pairs
#define MAX_QUEUE 100  //maximum number of queued synchronization tasks
#define MAX_WORKERS 5
#define PIPE_IN "fss_in"
#define PIPE_OUT "fss_out"
#define CONFIG_FILE "config.txt"
#define MANAGER_LOG "manager_log.txt"
#define EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + NAME_MAX + 1))  //buffer size for reading inotify events 



typedef struct{
    int errors;
    int active;
    char src[256];
    char dst[256];
    char last_status[32];
    char last_sync[64];
}sync_pair;


typedef struct{
    char src_path[PATH_MAX];
    char trg_path[PATH_MAX];
}worker_task;


extern int q_start;
extern int q_end;
extern int active_workers;
extern int out_fd;
extern int pair_total;  //total number of monitored pairs
extern sync_pair pair_list[MAX_PAIRS];
extern worker_task workers_queue[MAX_QUEUE];
extern FILE *manager_log_file;
extern volatile sig_atomic_t worker_done;


#endif