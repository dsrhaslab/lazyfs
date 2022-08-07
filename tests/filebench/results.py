#!/bin/python3

import argparse
import sys
import os
import prettytable
import statistics
import re
import pandas as pd
import math

def is_float(element):
    try:
        float(element)
        return True
    except ValueError:
        return False

def convert_size(size_bytes):
   if size_bytes == 0:
       return "0B"
   size_name = ("B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB")
   i = int(math.floor(math.log(size_bytes, 1024)))
   p = math.pow(1024, i)
   s = round(size_bytes / p, 2)
   return "%s %s" % (s, size_name[i])

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

filebench_table = prettytable.PrettyTable()

filebench_table.field_names = ["workload", "filesystem", "ops", "ops/s", "rd/wr", "mb/s", "ms/op"]

print ("> generating filebench table...")

for test_output_file in os.listdir(results_folder):
    
    if re.search("\.table$", test_output_file) or re.search("\.dstat\.csv$", test_output_file):
        continue
    
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
    list_nr_ops = []
    list_ops_s = []
    list_rd_wr_1 = []
    list_rd_wr_2 = []
    list_mb_s = []
    list_ms_op = []
    for summary in io_summaries:
        sum_parts = summary.split()
        list_nr_ops.append(float(sum_parts[3]))
        list_ops_s.append(float(sum_parts[5]))
        list_rd_wr_1.append(float(sum_parts[7].split('/')[0]))
        list_rd_wr_2.append(float(sum_parts[3].split('/')[0]))
        list_mb_s.append(float(sum_parts[9].replace("mb/s", "")))
        list_ms_op.append(float(sum_parts[10].replace("ms/op", "")))
        count_summary = count_summary + 1

    if count_summary > 0:

        avg_ops = round(statistics.mean(list_nr_ops),3)
        stdev_ops = round(statistics.stdev(list_nr_ops), 3)
        avg_ops_s = round(statistics.mean(list_ops_s),3)
        stdev_ops_s = round(statistics.stdev(list_ops_s), 3)
        avg_rd = round(statistics.mean(list_rd_wr_1),3)
        stdev_rd = round(statistics.stdev(list_rd_wr_1), 3)
        avg_wr = round(statistics.mean(list_rd_wr_2),3)
        stdev_wr = round(statistics.stdev(list_rd_wr_2), 3)
        avg_mb_s = round(statistics.mean(list_mb_s),3)
        stdev_mb_s = round(statistics.stdev(list_mb_s), 3)
        avg_ms_ops = round(statistics.mean(list_ms_op),3)
        stdev_ms_ops = round(statistics.stdev(list_ms_op), 3)

        filebench_table.add_row([workload, 
                            filesystem, 
                            "{} (+-{})".format(avg_ops, stdev_ops),
                            "{} (+-{})".format(avg_ops_s, stdev_ops_s), 
                            "{}/{} (+-{}/+-{})".format(avg_rd, avg_wr, stdev_rd, stdev_wr),
                            "{} (+-{})".format(avg_mb_s, stdev_mb_s),
                            "{} (+-{})".format(avg_ms_ops, stdev_ms_ops)])

filebench_table.sortby = "workload"
print(filebench_table)

print ("> generating metrics table...")

metrics_table = prettytable.PrettyTable()

metrics_table.field_names = ["workload", "filesystem", "cpu", "ram"]

line_counter = 0
for test_dstat_file in os.listdir(results_folder):
    
    line_counter = 0

    if not re.search("\.csv$", test_dstat_file):
        continue

    # run-WL-FS-fb.output
    partswl = test_dstat_file.split("-")
    workload = partswl[1]
    filesystem = ""
    add_idx = 0
    if workload == "seq" or workload == "rand":
        workload = partswl[1] + partswl[2]
        filesystem = partswl[3]
        add_idx = 1
    else:
        filesystem = partswl[2]

    if workload in micro_meta:
        workload = "micro_meta/"+workload
    elif workload in micro_read:
        workload = "micro_read/"+workload
    elif workload in micro_write:
        workload = "micro_write/"+workload
    elif workload in macro:
        workload = "macro/"+workload

    if filesystem == "fuse.lazyfs":        
        filesystem = filesystem + " (" + str(int(partswl[3+add_idx])/1024) + " KiB)"

    results_file_fd = open(results_folder+"/"+test_dstat_file, "r")
    result_lines = results_file_fd.readlines()

    table_list = []
    table_header = []
    
    for line in result_lines:
        line_counter = line_counter + 1
        if line_counter < 6:
            continue
        parts = line.split(",")
        if len(parts) < 10:
            continue
        if line_counter == 6:
            table_header=parts[1:]
        else:
            parsed_list = parts[:-1]
            parsed_list = parts[1:]
            new_list = [float(item) for subitem in parsed_list for item in subitem.split() if is_float(item)]
            table_list.append(new_list)

    df = pd.DataFrame(table_list, columns = table_header)

    avg_cpu = df['"usr"'].mean()
    std_cpu = df['"usr"'].std()
    avg_ram = convert_size(df['"used"'].mean())
    std_ram = convert_size(df['"used"'].std())

    metrics_table.add_row([workload, 
                            filesystem,
                            "{} (+-{})".format(avg_cpu, std_cpu, df['"used"'].mean()),
                            "{} (+-{}) -> {} bytes".format(avg_ram, std_ram, df['"used"'].std())
                            ])

metrics_table.sortby = "workload"
print(metrics_table)