#!/bin/bash
#===============================================================================
#
#   DESCRIPTION: This script contains useful functions for reproducing LazyFS bugs.
#
#        AUTHOR: Maria Ramos,
#       CREATED: 7 Mar 2024,
#      REVISION: [Date of Last Revision]
#
#
#===============================================================================

# Define color codes for printing text.
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
RESET='\033[0m'  

# Description: If the directory exists, removes all its content. Creates the directory if it exists.
# Param: Path of the directory.
function create_and_clean_directory() {
    local dir=$1
    if [ ! -d "$dir" ]; then
        mkdir -p "$dir"
        echo "Directory \"$dir\" created."
    else
        echo "Directory \"$dir\" already exists."
        rm -rf "$dir"/*
        echo "Contents removed from directory \"$dir\"."
    fi
}

# Description: If the file exists, removes it.
# Param: Path of the file.
function remove_file() {
    local file=$1
    if [ -e "$file" ]; then
        rm "$file"
        echo "File \"$file\" removed:."
    else
        echo "File \"$file\" does not exist."
    fi
}

# Description: Checks if the output produced by the application corresponds to the the expectation.
# Param: Expected output.
# Param: File that contains the output.
function check_error() {
    local expected_error=$1
    local target_file=$2

    if grep -q "$expected_error" "$target_file"; then
        echo -e "${GREEN}Error expected detected${RESET}!"
    else 
        echo -e "${RED}Error expected not detected${RESET}!"
    fi
}

# Description: Wait for some action to complete. Only returns when it is completed.
# Param: String produced when the action is completed.
# Param: File where the string is written.
function wait_action() {
    local target_string=$1
    local target_file=$2
    
    while ! grep -q "$target_string" "$target_file" 
    do 
        #echo -e "${YELLOW}Waiting...${RESET}"
        sleep 0.5
    done    
}
