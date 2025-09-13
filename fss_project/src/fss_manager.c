#include "../include/fss_manager.h"
#include "../include/inotify_utils.h"
#include "../include/manager_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <poll.h>


int max_workers = MAX_WORKERS;
char manager_log_path[PATH_MAX];
char config_file_path[PATH_MAX];


int main(int argc, char *argv[]){

    init_inotify();

    strcpy(manager_log_path, MANAGER_LOG);
    strcpy(config_file_path, CONFIG_FILE);

    //parse command-line arguments 
    int option;
    while((option = getopt(argc, argv, "l:c:n:")) != -1){
        switch(option){
            case 'l':
                strncpy(manager_log_path, optarg, sizeof(manager_log_path) - 1);
                manager_log_path[sizeof(manager_log_path) - 1] = '\0';
                break;
            case 'c':
                strncpy(config_file_path, optarg, sizeof(config_file_path) - 1);
                config_file_path[sizeof(config_file_path) - 1] = '\0';
                break;
            case 'n':
                max_workers = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-l log_file] [-c config_file] [-n worker_limit]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }


    //setup signal handler for SIGCHLD (child process termination)
    struct sigaction sa; 
    sa.sa_handler = child_signal_handler; //call the handler function when a child process terminates
    sigemptyset(&sa.sa_mask); //do not block any other signals while the handler is running 
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;  
    if(sigaction(SIGCHLD, &sa, NULL) == -1){  //install the handler for the SIGCHLD signal
        perror("sigaction");
        exit(1);
    }

    manager_log_file = fopen(manager_log_path, "a"); //open log file
    if(!manager_log_file){
        perror("log file");
        exit(1);
    }

    log_and_print("[MANAGER STARTED]");

    //create and clean named pipes if needed
    unlink(PIPE_IN);
    unlink(PIPE_OUT);
    if(mkfifo(PIPE_IN, 0666) == -1 && errno != EEXIST){
        perror("mkfifo input");
        exit(1);
    }

    if(mkfifo(PIPE_OUT, 0666) == -1 && errno != EEXIST){
        perror("mkfifo output");
        exit(1);
    }

    //open communication pipes
    int pipe_in = open(PIPE_IN, O_RDONLY);
    int pipe_out = open(PIPE_OUT, O_WRONLY);
    if(pipe_in < 0 || pipe_out < 0){
        perror("pipe open");
        exit(1);
    }

    out_fd = pipe_out;

    load_config(config_file_path); //load config file and add watches 

    dispatch_workers(out_fd); //start initial workers

    //setup poll to monitor pipe and inotify fd 
    struct pollfd fds[2];
    fds[0].fd = pipe_in;
    fds[0].events = POLLIN;
    fds[1].fd = inotify_fd;
    fds[1].events = POLLIN;

    while(1){

        int ret;
        do{
            ret = poll(fds, 2, -1);  //infinite timeout
        } while (ret == -1 && errno == EINTR); //retry if interrupted by signal 

        if(ret == -1){
            perror("poll");
            break;
        }

        //handle input from console 
        if(fds[0].revents & POLLIN){
            char buf[256];
            memset(buf, 0, sizeof(buf));
            int n = read(pipe_in, buf, sizeof(buf) - 1);
            if(n > 0){
                buf[n] = '\0';
    
                if(strncmp(buf, "shutdown", 8) == 0){
                    log_and_print("[MANAGER] Shutting down...");
                    dprintf(pipe_out, "Shutting down manager...\n");
                    break;
                }
                handle_command(buf, pipe_out);
            }
        }

        //handle file system events
        if(fds[1].revents & POLLIN){
            handle_inotify_events();
        }
    
        //handle completed workers 
        if(worker_done){
            dispatch_workers(out_fd);
            worker_done = 0;
        }

    }

    close(pipe_in);
    close(pipe_out);
    fclose(manager_log_file);
    unlink(PIPE_IN);
    unlink(PIPE_OUT);

    return 0;
}
