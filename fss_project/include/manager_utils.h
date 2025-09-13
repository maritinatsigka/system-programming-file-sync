#ifndef MANAGER_UTILS_H
#include <stdarg.h>
#define MANAGER_UTILS_H

void load_config(const char *filename); //loads synchronization pairs from the config file into memory 
void handle_command(const char *cmd, int out_fd); //processes a command received from fss_console and sends a response 
void queue_sync_task(const char *source_path, const char *target_path); //adds a new synchronization task into the workers queue
void dispatch_workers(int output_fd); //starts new worker processes for pending tasks, respecting the worker limit 
void child_signal_handler(int signal_number); //signal handler for SIGCHLD to detect when workers finish 

void log_msg(const char *message); //logs a simple message to the manager log file
void log_and_print(const char *format, ...); //logs a formatted message to both the screen and manager log file

//logs a detailed report of a worker's synchronization job 
void log_worker_report(const char *src, const char *trg, const char *filename, const char *operation, const char *status, const char *details, pid_t pid);

#endif