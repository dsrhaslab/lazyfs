#!/usr/bin/python3

import sys
import os
import re
import matplotlib.pyplot as plt

if len(sys.argv) != 3:
    print("error: run ./dstatgraph.py <folder> <arg_workload>")
    sys.exit(-1)

arg_dstat_folder = sys.argv[1]
arg_workload = sys.argv[2]

if not os.path.exists(arg_dstat_folder):
    print("error: folder '%s' does not exist!" % arg_dstat_folder)
    sys.exit(-1)

valid_workloads = ["varmail", "rand-read", "fileserver", "seq-read", "rand-write", "seq-write", "create", "delete", "stat", "webserver"]

if not arg_workload in valid_workloads:
    print("error: arg_workload '%s' invalid!" % arg_workload)
    print("valid workloads:", valid_workloads)
    sys.exit(-1)

fig, axs = plt.subplots(3, 2)

def import_dstat_from_file(path, labelname):
    count_host_lines = 0
    accumulate_dstat = []
    with open(path) as f:
        lines = f.readlines()
        for l in lines:
            if count_host_lines < 2:
                if re.search(".*Host.*", l):
                    count_host_lines = count_host_lines + 1
                elif not re.search(".*Cmdline.*", l) and not re.search(".*system.*", l) and not re.search(".*time.*", l) and not len(l) < 10:
                    accumulate_dstat.append(l)

    v_time_list = []
    v_usr_list = []
    v_read_list = []
    v_writ_list = []
    v_cach_list = []
    v_used_list = []
    v_free_list = []

    seconds_counter = 0

    for dstat_line in accumulate_dstat:

        if seconds_counter % 10 == 0:

            parts = dstat_line.split(",")
            
            v_time = parts[0]; v_usr = float(parts[1])
            v_sys = float(parts[2]); v_idl = float(parts[3])
            v_wai = float(parts[4]); v_stl = float(parts[5])
            v_read = float(parts[6]); v_writ = float(parts[7])
            v_used = float(parts[8]); v_free = float(parts[9])
            v_buff = float(parts[10]); v_cach = float(parts[11])
            v_in = float(parts[12]); v_out = float(parts[13])

            # Use seconds counter instead of the actual registered date
            # v_datetime = datetime.strptime(v_time, '%d-%m %H:%M:%S')

            v_time_list.append(seconds_counter)
            v_usr_list.append(v_usr)
            v_used_list.append(v_used)
            v_writ_list.append(v_writ)
            v_read_list.append(v_read)
            v_cach_list.append(v_cach)
            v_free_list.append(v_free)

        seconds_counter = seconds_counter + 1

    if labelname == "passthrough":
        axs[0,0].plot(v_time_list, v_usr_list, '--', label = labelname, color="red")
        axs[0,1].plot(v_time_list, v_used_list, '--', label = labelname, color="red")
        axs[1,0].plot(v_time_list, v_writ_list, '--', label = labelname, color="red")
        axs[1,1].plot(v_time_list, v_read_list, '--', label = labelname, color="red")
        axs[2,0].plot(v_time_list, v_cach_list, '--', label = labelname, color="red")
        axs[2,1].plot(v_time_list, v_free_list, '--', label = labelname, color="red")
    elif labelname == "lazyfs-4096":
        axs[0,0].plot(v_time_list, v_usr_list, '-.', label = labelname, color="grey")
        axs[0,1].plot(v_time_list, v_used_list, '-.', label = labelname, color="grey")
        axs[1,0].plot(v_time_list, v_writ_list, '-.', label = labelname, color="grey")
        axs[1,1].plot(v_time_list, v_read_list, '-.', label = labelname, color="grey")
        axs[2,0].plot(v_time_list, v_cach_list, '-.', label = labelname, color="grey")
        axs[2,1].plot(v_time_list, v_free_list, '-.', label = labelname, color="grey")
    elif labelname == "lazyfs-32768":
        axs[0,0].plot(v_time_list, v_usr_list, '-', label = labelname, color="black")
        axs[0,1].plot(v_time_list, v_used_list, '-', label = labelname, color="black")
        axs[1,0].plot(v_time_list, v_writ_list, '-', label = labelname, color="black")
        axs[1,1].plot(v_time_list, v_read_list, '-', label = labelname, color="black")
        axs[2,0].plot(v_time_list, v_cach_list, '-', label = labelname, color="black")
        axs[2,1].plot(v_time_list, v_free_list, '-', label = labelname, color="black")

    axs[0,0].set_xlim(0, seconds_counter)
    axs[0,1].set_xlim(0, seconds_counter)
    axs[1,0].set_xlim(0, seconds_counter)
    axs[1,1].set_xlim(0, seconds_counter)
    axs[2,0].set_xlim(0, seconds_counter)
    axs[2,1].set_xlim(0, seconds_counter)

    # plot cpu subgraph

    axs[0,0].set_ylabel("CPU (%)")
    axs[0,0].set_xlabel("Time (s)")
    axs[0,0].legend()

    # plot mem subgraph

    axs[0,1].set_ylabel("RAM Used (bytes)")
    axs[0,1].set_xlabel("Time (s)")
    axs[0,1].legend()

    # plot disk write access subgraph

    axs[1,0].set_ylabel("Disk Write (bytes)")
    axs[1,0].set_xlabel("Time (s)")
    axs[1,0].legend()

    # plot disk read access subgraph

    axs[1,1].set_ylabel("Disk Read (bytes)")
    axs[1,1].set_xlabel("Time (s)")
    axs[1,1].legend()

    # plot disk cache used subgraph

    axs[2,0].set_ylabel("Used Disk Cache (bytes)")
    axs[2,0].set_xlabel("Time (s)")
    axs[2,0].legend()

    # plot disk cache free subgraph

    axs[2,1].set_ylabel("Free Disk Cache (bytes)")
    axs[2,1].set_xlabel("Time (s)")
    axs[2,1].legend()

for path in os.listdir(arg_dstat_folder):
    if re.search(".*{}.*lazyfs-4096.*.csv".format(arg_workload), path):
        import_dstat_from_file("%s/%s" % (arg_dstat_folder, path), "lazyfs-4096")
    if re.search(".*{}.*lazyfs-32768.*.csv".format(arg_workload), path):
        import_dstat_from_file("%s/%s" % (arg_dstat_folder, path), "lazyfs-32768")
    if re.search(".*{}.*passthrough.*.csv".format(arg_workload), path):
        import_dstat_from_file("%s/%s" % (arg_dstat_folder, path), "passthrough")

fig.suptitle("Workload: %s" % arg_workload)
#fig.set_size_inches(20, 20)
#fig.savefig("example-plot.png", dpi=200)
plt.show()