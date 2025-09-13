#include "../include/fss_manager.h"
#include "../include/manager_utils.h"
#include "../include/sync_list.h"
#include "../include/inotify_utils.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>


int active_workers = 0;
int queue_start = 0;
int queue_end = 0;
worker_task workers_queue[MAX_QUEUE];
FILE *manager_log_file = NULL;
int out_fd = -1;
volatile sig_atomic_t worker_done = 0;


//log a simple message with timestamp to the manager log file 
void log_msg(const char *msg){

    if(!manager_log_file){
        return;
    }

    time_t now = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%F %T", localtime(&now));
    fprintf(manager_log_file, "[%s] %s\n", ts, msg);
    fflush(manager_log_file);
}


//print a message to stdout and log it to the manager log file 
void log_and_print(const char *format, ...){
    va_list args1;
    va_list args2;
    va_start(args1, format);
    va_copy(args2, args1);

    vprintf(format, args1);
    printf("\n");

    vfprintf(manager_log_file, format, args2);
    fprintf(manager_log_file, "\n");

    fflush(manager_log_file);
    va_end(args1);
    va_end(args2);
}


//signal handler for SIGCHLD: called when a child process terminates 
void child_signal_handler(int signal_number){

    while(waitpid(-1, NULL, WNOHANG) > 0){
        active_workers--;
    }
    worker_done = 1;
}


//add a new sync task to the workers queue
void queue_sync_task(const char *source_path, const char *target_path){

    if((queue_end + 1) % MAX_QUEUE == queue_start){
        fprintf(manager_log_file, "[QUEUE] Full. Task dropped: %s -> %s\n", source_path, target_path);
        return;
    }

    strncpy(workers_queue[queue_end].src_path, source_path, PATH_MAX);
    strncpy(workers_queue[queue_end].trg_path, target_path, PATH_MAX);
    queue_end = (queue_end + 1) % MAX_QUEUE;

    fprintf(manager_log_file, "[QUEUE] Task queued: %s -> %s\n", source_path, target_path);
    fflush(manager_log_file);
}


//start worker processes from the queue if possible 
void dispatch_workers(int output_fd){

    while(active_workers < MAX_WORKERS && queue_start != queue_end){
        worker_task *current_task = &workers_queue[queue_start];

        int pipe_fd[2];
        if(pipe(pipe_fd) == -1){
            perror("pipe");
            return;
        }

        pid_t worker_pid = fork();

        if(worker_pid < 0){
            perror("fork failed");
            exit(1);
        }

        if(worker_pid == 0){
            //child process: set up stdout and exec worker 
            close(pipe_fd[0]);  //close read
            dup2(pipe_fd[1], STDOUT_FILENO); 
            close(pipe_fd[1]);
            execl("bin/worker", "worker", current_task->src_path, current_task->trg_path, "ALL", "FULL", NULL);
            perror("execl failed");
            exit(1);
        }

        //parent process
        close(pipe_fd[1]);  //close write
        active_workers++;

        char buffer[1024];
        ssize_t bytes = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
        close(pipe_fd[0]);

        char status_clean[32] = "UNKNOWN";
        char details_clean[1024] = "No details";
        
        if(bytes > 0){
            buffer[bytes] = '\0';

            char *status = strstr(buffer, "STATUS:");
            char *details = strstr(buffer, "DETAILS:");

            if(status){
                status += strlen("STATUS:");
                sscanf(status, "%31s", status_clean);
            }
            if(details){
                details += strlen("DETAILS:");
                sscanf(details, "%1023[^\n]", details_clean);
            }
        } else{
            strcpy(status_clean, "FAIL");
            strcpy(details_clean, "No output from worker");
        }

        log_worker_report(current_task->src_path, current_task->trg_path, "ALL", "FULL", status_clean, details_clean, worker_pid);

        fprintf(manager_log_file, "[SPAWN] Worker for: %s -> %s\n", current_task->src_path, current_task->trg_path);
        fflush(manager_log_file);
        queue_start = (queue_start + 1) % MAX_QUEUE;
        
    }
}


//load initial sync pairs from configuration file 
void load_config(const char *filename){

    FILE *config_file = fopen(filename, "r");
    if(!config_file){
        perror("Failed to open config file");
        exit(1);
    }

    char source_path[256];
    char target_path[256];
    char line[512];
    while(fgets(line, sizeof(line), config_file)){
        if(sscanf(line, "%s %s", source_path, target_path) == 2){
            if(add_sync_pair(source_path, target_path) == 1){
                queue_sync_task(source_path, target_path);
                add_watch(source_path); 
                fprintf(manager_log_file, "[CONFIG] Loaded pair: %s -> %s\n", source_path, target_path);
            } else{
                fprintf(manager_log_file, "[CONFIG] Skipped duplicate: %s -> %s\n", source_path, target_path);
            }
        }
    }

    fclose(config_file);
    fflush(manager_log_file);
}


//handle a command received from console 
void handle_command(const char *command_line, int output_fd){

    char command[32], source_path[256], target_path[256];
    int parsed_args = sscanf(command_line, "%s %s %s", command, source_path, target_path);

    if(parsed_args == 3 && strcmp(command, "add") == 0){

        if(access(source_path, F_OK) != 0){
            dprintf(output_fd, "EXEC_REPORT_START\n");
            dprintf(output_fd, "[ERROR] Source directory does not exist: %s\n", source_path);
            dprintf(output_fd, "EXEC_REPORT_END\n");
            fprintf(manager_log_file, "[ADD] Failed - Source not found: %s\n", source_path);
            fflush(manager_log_file);
            return;
        }
        
        if(access(target_path, F_OK) != 0){
            dprintf(output_fd, "EXEC_REPORT_START\n");
            dprintf(output_fd, "[ERROR] Target directory does not exist: %s\n", target_path);
            dprintf(output_fd, "EXEC_REPORT_END\n");
            fprintf(manager_log_file, "[ADD] Failed - Target not found: %s\n", target_path);
            fflush(manager_log_file);
            return;
        }

        int result = add_sync_pair(source_path, target_path);

        dprintf(output_fd, "EXEC_REPORT_START\n");

        if(result == 0){
            dprintf(output_fd, "Already in queue: %s\n", source_path);
            fprintf(manager_log_file, "[ADD] Duplicate ignored: %s\n", source_path);
        } else if(result == 1){
            queue_sync_task(source_path, target_path);
            add_watch(source_path); 
            dprintf(output_fd, "Added directory: %s -> %s\n", source_path, target_path);
            log_and_print("[ADD] New pair: %s -> %s", source_path, target_path);
        } else{
            dprintf(output_fd, "Add failed.\n");
        }

        dprintf(output_fd, "EXEC_REPORT_END\n");

    } else if(parsed_args == 2 && strcmp(command, "cancel") == 0){

        int res = cancel_sync_pair(source_path);
        dprintf(output_fd, "EXEC_REPORT_START\n");

        if(res){
            dprintf(output_fd, "Monitoring stopped for %s\n", source_path);
            fprintf(manager_log_file, "[CANCEL] %s cancelled.\n", source_path);
        } else{
            dprintf(output_fd, "Directory not monitored: %s\n", source_path);
        }
        dprintf(output_fd, "EXEC_REPORT_END\n");

    } else if(parsed_args == 2 && strcmp(command, "status") == 0){
        sync_node *entry = find_sync_pair(source_path);
        if(!entry){
            dprintf(output_fd, "Directory not monitored: %s\nEXEC_REPORT_END\n", source_path);
        } else{
            const char *status = entry->active ? "Active" : "Inactive";
            dprintf(output_fd,"EXEC_REPORT_START\n");
            dprintf(output_fd, "Directory: %s\n", entry->src);
            dprintf(output_fd,"Target: %s\n", entry->trg);
            dprintf(output_fd, "Last Sync: %s\n", entry->last_sync);
            dprintf(output_fd, "Errors: %d\n", entry->errors);
            dprintf(output_fd, "Status: %s\n", status);
            dprintf(output_fd, "EXEC_REPORT_END\n");
    }

    } else if(parsed_args == 2 && strcmp(command, "sync") == 0){
        char target_path[PATH_MAX];
        int result = start_manual_sync(source_path, target_path);

        dprintf(output_fd, "EXEC_REPORT_START\n");
        if(result == 0){
            dprintf(output_fd, "Directory not monitored: %s\n", source_path);
        } else if(result == -1){
            dprintf(output_fd, "Sync already in progress %s\n", source_path);
        } else{
            queue_sync_task(source_path, target_path);
            dprintf(output_fd, "Syncing directory: %s -> %s\n", source_path, target_path);
            log_and_print("[SYNC] Manual sync started: %s -> %s", source_path, target_path);
        }

        dprintf(output_fd, "EXEC_REPORT_END\n");

    } else{
        dprintf(output_fd,"EXEC_REPORT_START\n");
        dprintf(output_fd,"Invalid or unsupported command.\n");
        fprintf(manager_log_file, "[COMMAND ERROR] Unknown input: %s\n", command_line);
        dprintf(output_fd, "EXEC_REPORT_END\n");
    }

    fflush(manager_log_file);
}


//log a summary report of worker completion 
void log_worker_report(const char *src, const char *trg, const char *filename, const char *operation, const char *status, const char *details, pid_t pid){
    
    if(!manager_log_file){
        return;
    }

    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%F %T", localtime(&now));

    //format details differently if specific file is involved
    char final_details[512];
    if(strcmp(filename, "ALL") != 0){
        snprintf(final_details, sizeof(final_details), "File: %s %s", filename, details);
    } else{
        snprintf(final_details, sizeof(final_details), "%s", details);
    }

    fprintf(manager_log_file, "[%s] [%s] [%s] [%d] [%s] [%s] [%s]\n", timestamp, src, trg, pid, operation, status, final_details);

    fflush(manager_log_file);
}