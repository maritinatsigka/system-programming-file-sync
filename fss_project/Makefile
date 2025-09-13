CC = gcc
CFLAGS = -Wall -Iinclude
SRC_DIR = src
BIN_DIR = bin


MANAGER_SRC = $(SRC_DIR)/fss_manager.c $(SRC_DIR)/manager_utils.c $(SRC_DIR)/sync_list.c $(SRC_DIR)/inotify_utils.c
CONSOLE_SRC = $(SRC_DIR)/fss_console.c
WORKER_SRC = $(SRC_DIR)/worker.c


MANAGER_BIN = $(BIN_DIR)/fss_manager
CONSOLE_BIN = $(BIN_DIR)/fss_console
WORKER_BIN = $(BIN_DIR)/worker


all: $(MANAGER_BIN) $(CONSOLE_BIN) $(WORKER_BIN)


$(MANAGER_BIN): $(MANAGER_SRC)
	$(CC) $(CFLAGS) -o $@ $^

$(CONSOLE_BIN): $(CONSOLE_SRC)
	$(CC) $(CFLAGS) -o $@ $^

$(WORKER_BIN): $(WORKER_SRC)
	$(CC) $(CFLAGS) -o $@ $^


clean:
	rm -f $(BIN_DIR)/*

.PHONY: all clean