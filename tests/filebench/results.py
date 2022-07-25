#!/bin/python3

import argparse
import sys
import os
import prettytable
import statistics

if len(sys.argv) != 2:
    print("./results.py output_folder")
    sys.exit()

results_folder = sys.argv[1]

if not os.path.isdir(results_folder):
    print("The specified output '", results_folder, "' is not a folder")

micro_meta = ["create", "stat", "delete"]
micro_read = ["randread", "seqread"]
micro_write = ["randwrite", "seqwrite"]
macro = ["fileserver", "varmail", "webserver"]

results_table = prettytable.PrettyTable()

results_table.field_names = ["workload", "filesystem", "ops", "ops/s", "rd/wr", "mb/s", "ms/op"]

for test_output_file in os.listdir(results_folder):
    
    # run-WL-FS-fb.output
    parts = test_output_file.split("-")
    workload = parts[1]
    filesystem = ""
    add_idx = 0
    if workload == "seq" or workload == "rand":
        workload = parts[1] + parts[2]
        filesystem = parts[3]
        add_idx = 1
    else:
        filesystem = parts[2]

    if workload in micro_meta:
        workload = "micro_meta/"+workload
    elif workload in micro_read:
        workload = "micro_read/"+workload
    elif workload in micro_write:
        workload = "micro_write/"+workload
    elif workload in macro:
        workload = "macro/"+workload

    if filesystem == "fuse.lazyfs":        
        filesystem = filesystem + " (" + str(int(parts[3+add_idx])/1024) + " KiB)"

    results_file_fd = open(results_folder+"/"+test_output_file, "r")
    result_lines = results_file_fd.readlines()
    io_summaries = list(filter(lambda line: "IO Summary" in line, result_lines))

    count_summary = 0
    sum_nr_ops = 0
    sum_ops_s = 0
    sum_rd_wr_1 = 0
    sum_rd_wr_2 = 0
    sum_mb_s = 0
    sum_ms_op = 0
    for summary in io_summaries:
        sum_parts = summary.split()
        sum_nr_ops = sum_nr_ops + float(sum_parts[3])
        sum_ops_s = sum_ops_s + float(sum_parts[5])
        sum_rd_wr_1 = sum_rd_wr_1 + float(sum_parts[7].split('/')[0])
        sum_rd_wr_2 = sum_rd_wr_2 + float(sum_parts[7].split('/')[1])
        sum_mb_s = sum_mb_s + float(sum_parts[9].replace("mb/s", ""))
        sum_ms_op = sum_ms_op + float(sum_parts[10].replace("ms/op", ""))
        count_summary = count_summary + 1

    if count_summary > 0:

        results_table.add_row([workload, 
                            filesystem, 
                            round(float(sum_nr_ops/count_summary),3), 
                            round(float(sum_ops_s/count_summary),3), 
                            str(round(float(sum_rd_wr_1/count_summary),3))+"/"+str(round(float(sum_rd_wr_2/count_summary),3)), 
                            round(float(sum_mb_s/count_summary),3),
                            round(float(sum_ms_op/count_summary),3)])

results_table.sortby = "workload"
print(results_table)