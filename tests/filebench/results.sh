#!/bin/bash

#----------------------------------------------------------
# Generates the results folder for 'WORKLOAD_NAME'
# It uses the template-tests folder to generate a
# new results folder
#----------------------------------------------------------

WORKLOAD_NAME="filemicro_rwrite"
OUTPUT_FOLDER="general-tests/$WORKLOAD_NAME"

#----------------------------------------------------------

cp -r template-tests $OUTPUT_FOLDER
rename "s/template-tests/$WORKLOAD_NAME/g" $OUTPUT_FOLDER/*
sed -i "s/template-tests/$WORKLOAD_NAME/g" $OUTPUT_FOLDER/*

#----------------------------------------------------------
