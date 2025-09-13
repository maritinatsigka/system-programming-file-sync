#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>
#include <errno.h>

#define PIPE_IN "fss_in" //named pipe for sending commands to manager 
#define PIPE_OUT "fss_out" //named pipe for receiving responses from manager 
#define MAX_INPUT 512
#define MAX_RESPONSE 2048


//function to open the named pipes for communication 
int setup_pipes(int *fd_in, int *fd_out){
    *fd_in = open(PIPE_IN, O_WRONLY);
    *fd_out = open(PIPE_OUT, O_RDONLY);
    return (*fd_in >= 0 && *fd_out >= 0) ? 0 : -1;
}


//function to get the current timestamp in a specific format 
void get_timestamp(char *ts, size_t size){
    time_t now = time(NULL);
    strftime(ts, size, "%Y-%m-%d %H:%M:%S", localtime(&now));
}


//function to read manager's response from pipe and log it 
void read_and_log_response(int out_fd, FILE *log_file){

    char reply[MAX_RESPONSE];
    int done = 0;

    puts("Manager says:\n");

    while(!done){
        memset(reply, 0, sizeof(reply));
        int n = read(out_fd, reply, sizeof(reply) - 1);

        if(n > 0){
            reply[n] = '\0';

            char *line = strtok(reply, "\n");
            while(line != NULL){
                if(strcmp(line, "EXEC_REPORT_END") == 0){
                    done = 1;
                    break;
                }

                if(strcmp(line, "EXEC_REPORT_START") != 0){
                    printf("%s\n", line);

                    char ts[64];
                    time_t now = time(NULL);
                    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
                    fprintf(log_file, "[%s] Response %s\n", ts, line);
                }

                line = strtok(NULL, "\n");
            }
        } else{
            // Αν δεν έχει τίποτα να διαβάσει, βγες με σφάλμα
            perror("read");
            break;
        }
    }

    fflush(log_file);

}


int main(int argc, char *argv[]){
    char *logname = NULL;
    int opt;

    //parse command-line arguments 
    while((opt = getopt(argc, argv, "l:")) != -1){
        if(opt == 'l'){
            logname = optarg;
        } else{
            fprintf(stderr, "Usage: %s -l <log_file>\n", argv[0]);
            exit(1);
        }
    }

    if(!logname){
        fprintf(stderr, "Log file not specified.\n");
        exit(1);
    }

    FILE *log_file = fopen(logname, "a");
    if(!log_file){
        perror("fopen");
        exit(1);
    }

    int in_fd;
    int out_fd;

    //setup communication pipes
    if(setup_pipes(&in_fd, &out_fd) != 0){
        perror("pipe open");
        fclose(log_file);
        exit(1);
    }

    printf("fss_console ready (type 'shutdown' to exit).\n");

    char input[MAX_INPUT];

    while(1){
        printf("> ");
        fflush(stdout);

        if(fgets(input, sizeof(input), stdin) == NULL){
            printf("\n"); 
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        if(strlen(input) == 0){
            continue;
        }

        //log the user command 
        char ts[64];
        get_timestamp(ts, sizeof(ts));
        fprintf(log_file, "[%s] Command %s\n", ts, input);
        fflush(log_file);

        //send command to manager 
        if(dprintf(in_fd, "%s\n", input) < 0){
            perror("write to pipe");
            break;
        }

        //read and print manager's response 
        read_and_log_response(out_fd, log_file);

        //exit if shutdown command is entered 
        if(strcmp(input, "shutdown") == 0){
            break;
        }
    }   

    close(in_fd);
    close(out_fd);
    fclose(log_file);
    return 0;
}