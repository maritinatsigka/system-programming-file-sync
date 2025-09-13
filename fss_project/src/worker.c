#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <limits.h>

#define BUF_SIZE 1024
#define ERR_BUF_SIZE 4096


//copy a file from source to target (returns 1 on success, 0 on failure)
int copy_file(const char *src, const char *trg, char *err_buf, int *errors){

    int fd_src = open(src, O_RDONLY);
    if(fd_src < 0){
        //handle source file open error
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to open source: %s (%s)\n", src, strerror(errno));
        strncat(err_buf, msg, ERR_BUF_SIZE - strlen(err_buf) - 1);
        err_buf[ERR_BUF_SIZE - 1] = '\0';
        (*errors)++;
        return 0;
    }

    int fd_trg = open(trg, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd_trg < 0){
        //handle target file open error 
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to open destination: %s (%s)\n", trg, strerror(errno));
        strncat(err_buf, msg, ERR_BUF_SIZE - strlen(err_buf) - 1);
        err_buf[ERR_BUF_SIZE - 1] = '\0';
        close(fd_src);
        (*errors)++;
        return 0;
    }

    //read from source and write to target
    char buffer[BUF_SIZE];
    ssize_t bytes;
    while((bytes = read(fd_src, buffer, BUF_SIZE)) > 0){
        if(write(fd_trg, buffer, bytes) != bytes){
            //handle write error 
            char msg[256];
            snprintf(msg, sizeof(msg), "Write error on: %s (%s)\n", trg, strerror(errno));
            strncat(err_buf, msg, ERR_BUF_SIZE - strlen(err_buf) - 1);
            err_buf[ERR_BUF_SIZE - 1] = '\0';
            (*errors)++;
            break;
        }
    }

    close(fd_src);
    close(fd_trg);
    return 1;
}


//perform a full synchronization of all regular files from source to target directory 
void perform_full_sync(const char *src_dir, const char *trg_dir){

    DIR *src = opendir(src_dir);
    if(!src){
        //report failure to open source directory 
        printf("EXEC_REPORT_START\nSTATUS: ERROR\nDETAILS: Cannot open source dir %s (%s)\nEXEC_REPORT_END\n", src_dir, strerror(errno));
        return;
    }

    struct dirent *entry;
    int copied = 0, errors = 0;
    char err_buf[ERR_BUF_SIZE] = "";

    while((entry = readdir(src)) != NULL){
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
            continue;
        }

        char full_src[PATH_MAX], full_trg[PATH_MAX];
        snprintf(full_src, sizeof(full_src), "%s/%s", src_dir, entry->d_name);
        snprintf(full_trg, sizeof(full_trg), "%s/%s", trg_dir, entry->d_name);

        struct stat st;
        if (stat(full_src, &st) == -1 || !S_ISREG(st.st_mode)){
            //skip non-regular files 
            continue;
        }
        if (copy_file(full_src, full_trg, err_buf, &errors)) {
            copied++;
        }
    }

    closedir(src);

    //determine final status 
    const char *status;
    if(errors == 0){
        status = "SUCCESS";
    }
    else if(copied > 0){
        status = "PARTIAL";
    }
    else{
        status = "ERROR";
    }

    //print final EXEC_REPORT
    printf("EXEC_REPORT_START\n");
    printf("STATUS: %s\n", status);
    printf("DETAILS: %d files copied, %d failed\n", copied, errors);
    if(strlen(err_buf) > 0){
        printf("ERRORS: %s", err_buf);
    }
        
    printf("EXEC_REPORT_END\n");
}


int main(int argc, char *argv[]){
    if(argc != 5){
        fprintf(stderr, "Usage: %s <src_dir> <trg_dir> <filename|ALL> <operation>\n", argv[0]);
        return 1;
    }

    const char *src_dir = argv[1];
    const char *trg_dir = argv[2];
    const char *filename = argv[3];
    const char *operation = argv[4];

    //handle FULL operation 
    if(strcmp(operation, "FULL") == 0 && strcmp(filename, "ALL") == 0){
        perform_full_sync(src_dir, trg_dir);
    } else if(strcmp(operation, "ADDED") == 0 || strcmp(operation, "MODIFIED") == 0){ //handle file addition or modification 

        char full_src[PATH_MAX], full_trg[PATH_MAX];
        snprintf(full_src, sizeof(full_src), "%s/%s", src_dir, filename);
        snprintf(full_trg, sizeof(full_trg), "%s/%s", trg_dir, filename);

        char err_buf[ERR_BUF_SIZE] = "";
        int errors = 0;

        if(copy_file(full_src, full_trg, err_buf, &errors)){
            printf("STATUS: SUCCESS\n");
            printf("DETAILS: File: %s %s\n", filename, strcmp(operation, "ADDED") == 0 ? "added" : "modified");
        } else{
            printf("STATUS: ERROR\n");
            printf("DETAILS: File: %s  Failed to %s file: %s\n", filename, operation, filename);
            if(strlen(err_buf) > 0){
                printf("ERRORS: %s\n", err_buf);
            }
        }
    } else if(strcmp(operation, "DELETED") == 0){ //handle file deletion 
        char full_trg[PATH_MAX];
        snprintf(full_trg, sizeof(full_trg), "%s/%s", trg_dir, filename);

        if(unlink(full_trg) == 0){
            printf("STATUS: SUCCESS\n");
            printf("DETAILS: File: %s deleted\n", filename);
        } else{
            printf("STATUS: ERROR\n");
            printf("DETAILS: File: %s  Failed to delete file: %s (%s)\n", filename, filename, strerror(errno));
        }
    } else{ //handle unsupported operation 
        printf("STATUS: ERROR\n");
        printf("DETAILS: Unsupported operation: %s\n", operation);
    }

    printf("EXEC_REPORT_END\n");

    return 0;
}