#!/bin/bash

#script for listing sync directories and cleaning up

#show all sync operations from the log file
show_list_all() {
    echo ">>> Full sync list:"
    while IFS= read -r line; do
        if echo "$line" | grep -qE "\[(FULL|ADDED|MODIFIED|DELETED)\]"; then
            ts=$(echo "$line" | awk -F'[][]' '{print $2}')
            src=$(echo "$line" | awk -F'[][]' '{print $4}')
            trg=$(echo "$line" | awk -F'[][]' '{print $6}')
            status=$(echo "$line" | awk -F'[][]' '{print $10}')
            echo "$src -> $trg [Last Sync: $ts] [$status]"
        fi
    done < "$1"
}


#show currently monitored directories from the log 
show_monitored() {
    echo ">>> Currently monitored directories:"
    grep "\[ADD\] New pair:" "$1" | while IFS= read -r line; do
        ts=$(echo "$line" | awk -F'[][]' '{print $2}')
        src=$(echo "$line" | sed -n 's/.*New pair: \([^ ]*\) -> .*/\1/p')
        trg=$(echo "$line" | sed -n 's/.*New pair: [^ ]* -> \(.*\)/\1/p')
        if [ -n "$src" ] && [ -n "$trg" ]; then
            echo "$src -> $trg [Last Sync: $ts]"
        fi
    done
}

#show directrories that are no longer monitored 
show_stopped() {
    echo ">>> Directories no longer monitored:"
    grep "\[CANCEL\]" "$1" | while IFS= read -r line; do
        #extract timestamp and directory name 
        ts=$(echo "$line" | awk -F'[][]' '{print $2}')
        dir=$(echo "$line" | sed -n 's/.*Monitoring stopped for \(.*\)$/\1/p')
        #display stopped directory info 
        echo "$dir [Last Sync: $ts]"
    done
}


#delete a file or directory 
do_purge() {
    echo ">>> Deleting: $1"
    if [ -d "$1" ]; then
        rm -rf "$1"
        echo "Directory deleted."
    elif [ -f "$1" ]; then
        rm -f "$1"
        echo "File deleted."
    else
        echo "Nothing to delete."
    fi
}

#check if the correct number of arguments is provided 
if [ "$#" -ne 4 ]; then
    echo "Usage: $0 -p <path> -c <command>"
    exit 1
fi

#parse input arguments 
while getopts "p:c:" opt; do
    case $opt in
        p) path=$OPTARG ;;
        c) command=$OPTARG ;;
        *) echo "Invalid option."; exit 1 ;;
    esac
done


#check if the given path exists
if [ ! -e "$path" ]; then
    echo "Path does not exist: $path"
    exit 1
fi


#perform the requested action based on the command 
case $command in
    listAll) show_list_all "$path" ;;
    listMonitored) show_monitored "$path" ;;
    listStopped) show_stopped "$path" ;;
    purge) do_purge "$path" ;;
    *)
        #handle unknown commands 
        echo "Unknown command: $command"
        echo "Available commands: listAll, listMonitored, listStopped, purge"
        exit 1
        ;;
esac