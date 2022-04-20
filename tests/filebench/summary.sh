#!/bin/bash

#--------------------------------------------------------
# Edit this variables

RESULTS_FOLDER="general-tests"

#--------------------------------------------------------

printf '~%.0s' {1..76}
printf "\n| %12s | %12s | %15s | %9s | %12s |\n" "workload" "filesystem" "page size (kb)" "ops (#/s)" "rd/wr (mb/s)"
printf '~%.0s' {1..76}

results=""

for result_file in $(find $RESULTS_FOLDER/ -type f ! -name '*.table' -print)
do
    summaries=`grep 'Summary' $result_file`
    AVG_OPS=0
    AVG_RDWR=0
    while read -r line
    do  
        ops_raw=`grep -o "ops.*ops/s" <<< $line`
        rd_wr_raw=`grep -o "rd/wr.*mb/s" <<< $line`
        ops_val=`grep -Eo '[+-]?[0-9]+([.][0-9]+)?' <<< $ops_raw`
        rd_wr_val=`grep -Eo '[+-]?[0-9]+([.][0-9]+)?' <<< $rd_wr_raw`
        AVG_OPS=`awk "BEGIN{ print $AVG_OPS + $ops_val }"`
        AVG_RDWR=`awk "BEGIN{ print $AVG_RDWR + $rd_wr_val }"`
    done <<< "$summaries"
    
    AVG_OPS=`awk "BEGIN{ printf \"%.2f\", ($AVG_OPS / 3.0) }"`
    AVG_RDWR=`awk "BEGIN{ printf \"%.2f\", ($AVG_RDWR / 3.0) }"`
    
    workload_name=`grep -o "/.*/" <<< $result_file | tr -d '/'`
    app_name=`grep -o "lazyfs-" <<< $result_file | tr -d '-'`
    if [ -z $app_name ];
    then
        app_name="passthrough"
    fi
    page_size=`grep -o "page\-.*k" <<< $result_file | tr -d 'k' | tr -d '-'`
    if [ -z $page_size ];
    then
        page_size="-"
    else
        page_size=${page_size#"page"}
    fi
    curr=`printf "| %12s | %12s | %15s | %9s | %12s |" $workload_name $app_name $page_size $AVG_OPS $AVG_RDWR`
    results="$results$curr\n"
done

echo -e  "$results" | sort
printf '~%.0s' {1..76} && printf '\n'
