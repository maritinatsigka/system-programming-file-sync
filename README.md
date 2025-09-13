# File Synchronization System (FSS)

This repository contains my implementation of the **System Programming (2025)** course project at the National and Kapodistrian University of Athens (NKUA).  
The project is written in **C** and demonstrates low-level system programming concepts, including **process management, inter-process communication (IPC), named pipes, signals, and inotify-based filesystem monitoring**.

---

## 📌 Project Overview

The goal of this project was to design and implement a **File Synchronization Service (FSS)** that automatically synchronizes files between source and target directories in real time.  
The system ensures consistency, provides monitoring and reporting mechanisms, and allows user interaction via a dedicated console.

The system is composed of the following components:

- **fss_manager**  
  Core process that monitors source directories, manages synchronization tasks, and coordinates worker processes.  
  Communicates with the console via **named pipes (fss_in, fss_out)**.  
  Spawns workers with `fork/exec` and assigns them synchronization tasks.  

- **fss_console**  
  Command-line interface for user interaction.  
  Supports commands such as `add`, `cancel`, `status`, `sync`, and `shutdown`.  
  Logs user input and displays system responses in real time.  

- **worker**  
  Independent processes responsible for performing actual synchronization using **low-level system calls** (`open`, `read`, `write`, `unlink`).  
  Workers handle operations such as FULL, ADDED, MODIFIED, and DELETED, and report detailed results back to the manager.  

- **fss_script.sh**  
  Helper Bash script for reporting and cleanup.  
  Commands include:  
  - `listAll` → show all monitored directories with last sync status  
  - `listMonitored` → show active directories  
  - `listStopped` → show stopped directories  
  - `purge` → remove a backup directory or a log file  

---

## ⚙️ Features
- Real-time directory monitoring with **inotify**.  
- Communication between manager and console via **named pipes**.  
- Worker lifecycle management with **fork/exec** and **SIGCHLD** handling.  
- Queue-based scheduling for pending synchronization tasks.  
- Structured logging for both manager and console.  
- Configurable maximum number of concurrent workers.  
- Bash script utilities for reports and cleanup.  

---

## ▶️ Run Instructions
1. **Start the Manager**
   ```bash
   ./bin/fss_manager -l manager_log.txt -c config.txt -n 5
   ```
   - -l → manager log file
   - -c → configuration file with sync pairs (<source_dir> <target_dir>)
   - -n → maximum number of concurrent workers
3. **Start the Console**
   ```bash
   ./bin/fss_console -l console_log.txt
   ```
5. **Available Console Commands**
   - add <source> <target> → start monitoring and synchronizing a new directory pair
   - cancel <source> → stop monitoring a directory
   - status <source> → get synchronization status for a directory
   - sync <source> → trigger manual synchronization
   - shutdown → gracefully stop the manager and all workers
6. **Use the Helper Script**: bash fss_script.sh -p <logfile_or_directory> -c <command>
   - listAll
   - listMonitored
   - listStopped
   - purge
---
