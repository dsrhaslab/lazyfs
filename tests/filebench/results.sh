#!/bin/bash

#----------------------------------------------------------
# Generates the results folder for 'WORKLOAD_NAME'
# It uses the template-tests folder to generate a
# new results folder
#----------------------------------------------------------

while getopts w:o: flag
do
    case "${flag}" in
        w) workload_name=${OPTARG};;
        o) results_folder=${OPTARG};;
    esac
done

if [ -z $workload_name ];
then
    echo "specify workload name with '-w <workload_name>'"
    exit
fi

if [ -z $results_folder ];
then
    echo "specify results folder with '-o <results_folder>'"
    exit
fi


WORKLOAD_NAME="$workload_name"
OUTPUT_FOLDER="$results_folder/$WORKLOAD_NAME"

#----------------------------------------------------------

cp -r template-tests $OUTPUT_FOLDER
rename "s/template-tests/$WORKLOAD_NAME/g" $OUTPUT_FOLDER/*
sed -i "s/template-tests/$WORKLOAD_NAME/g" $OUTPUT_FOLDER/*

#----------------------------------------------------------
