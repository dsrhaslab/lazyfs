#!/usr/bin/python3
#coding=utf-8

import pydot
import re
import argparse
import sys

sys.setrecursionlimit(10**6)

ops = []
ops_grouped = [] 
buffer = []
ops_i = 0
buffer_i = 0
i = 0
limit_group = 4

log_file = '/tmp/lazyfs.log'
covered_syscalls = 'write|rename|fsync|fdatasync|create|open|release|read'
files = ''
text = False
filter_only = False

#Two operations are the same if they have the same syscall and path
def compare_ops(op1,op2): 
    if op1['syscall'] == 'rename':
         return (op2['syscall'] == rename and op1['from'] == op2['from'] and op1['to'] == op2['to'])
    else:
        return op1['syscall'] == op2['syscall'] and op1['path'] == op2['path'] 

def parse_ops():
    global log_file, ops
    with open(log_file) as f:
        lines = f.readlines()

    path = r'([^,]*)'
    from_ = r'(.*?)'
    to = r'(.*?)'
    if len(files)!=0:
        path = '(' + files + ')'
        from_ = '(' + files + ')'
        to = '(' + files + ')'
    for line in lines:  
        #.*\[lazyfs.ops\]:\s*lfs_((rename|write|open)\((path=([^,]*)|from=(.*?),to=(.*?)\)))(,size=(\d+),off=(\d+))?(,mode=([A-Z_\-]+))?\)
        if op:=re.match(r'.*\[lazyfs.ops\]:\s*lfs_((' + covered_syscalls + r')\((path=' + path + r'|from='+ from_ + r',to=' + to + r'\)))(,size=(\d+),off=(\d+))?(,mode=([A-Z_\-]+))?(,isdatasync=[01])?\)',line):
            op_parsed = {'syscall':op.group(2)}
            if op.group(2)!='rename':
                op_parsed['path'] = op.group(4)

            if op.group(2) == 'write' or op.group(2) == 'read':
                op_parsed['size'] = op.group(8)
                op_parsed['off'] = op.group(9)
            elif op.group(2) == 'rename':
                op_parsed['from_'] = op.group(3)
                op_parsed['to'] = op.group(4)
            elif op.group(2) == 'open' or op.group(2) == 'create':
                op_parsed['mode'] = op.group(11)

            ops.append(op_parsed)
        elif op:=re.match(r'.*?(Triggered fault condition).*?\n',line):
            ops.append({'syscall': 'fault'})

def group_ops(ops):
    global ops_i, buffer_i, i, limit_group, ops_grouped, buffer

    while ops_i < len(ops) and len(buffer) < limit_group:
            #print('ops',ops, ops_i)
            op = ops[ops_i]
            #print(buffer, op)
            if len(buffer) == 0:                    #If there's no pattern, we'll start one
                buffer.append([op,1])
                buffer_i = ops_i
            else:                                   #If there's a pattern being constructed
                if compare_ops(buffer[i][0],op):    #If the operation is the same as the one in the beggining of the pattern  
                    buffer[i][1]+=1                 #Increment the occurrence of that operation   
                    i+=1                            #Update i. Now, when we add an operation we will compare it with the operation in the i+1 position of the buffer 
                    
                    if i==len(buffer):              #If we found the whole pattern, i is set to 0, because the pattern can be repeated more times
                        i = 0
                else:
                    if i==0 and buffer[0][1]==1:    #If the operation is not the sames as the one in the beggining of the pattern, but we are still forming a pattern, we add this operation to the buffer
                        buffer.append([op,1])
                    else:                           #If i>0, it means that we were already on the process of repeating a pattern and now it was broken
                        buffer_to_ops_grouped()
                        
            ops_i+=1
                
    if len(buffer) > 0:                             #If there's operations in the buffer, we will copy them to ops_grouped 
        if buffer[0][1] > 2 or (buffer[0][1] == 2 and len(buffer) == 1): #If the pattern in the buffer was repeated (1st op has reps>2) or if the 1st op has 2 reps and is the only one in the buffer (case where the same op is repeated)
            group = {'syscall':'group','group':[]}
            for elem in buffer:
                if 'off' in elem[0]:
                    del elem[0]["off"]
                    del elem[0]["size"]
                if 'mode' in elem[0]:
                    del elem[0]['mode']
                group['group'].append(elem[0])
                buffer_i += elem[1]
            ops_grouped.append((group,buffer[0][1]))
        else:
            ops_grouped.append((buffer[0][0],1))
    
    buffer.clear()
    if buffer_i < len(ops)-1:
        ops_i = 0
        i = 0
        group_ops(ops[buffer_i+1:])                 #We'll try to build a pattern, but now starting in another operation    

def buffer_to_ops_grouped():
    global ops_i, buffer_i, i, limit_group, ops_grouped, buffer

    count_reps = buffer[0][1]                       #Check in what repetion we were (reps of the 1st op)
    right_count = buffer[len(buffer)-1][1]          #Check how many repetions of the pattern were actually done (reps of the last op)
    group1 = {'syscall':'group','group':[]}
    if count_reps==right_count or (count_reps==2 and compare_ops(buffer[0][0],buffer[1][0])): #If the 1st and last ops of the buffer have the same number of reps, we can copy those ops in ops_grouped and clean completely the buffer
        for elem in buffer:
            if 'off' in elem[0]:
                del elem[0]["off"]
                del elem[0]["size"]
            if 'mode' in elem[0]:
                del elem[0]['mode']
            group1['group'].append(elem[0])
        ops_grouped.append((group1,right_count))    
        ops_i-=1                                    #We want to keep "pointing" to the current op
    elif count_reps==2:                             #For ops=='abcabd' we want the result ((a,1),(b,1),(c,1),(a,1),(b,1),(d,1)), no group of ops was formed
        ops_grouped.append((buffer[0][0],1))
        
        for o in buffer:
            ops_i -= o[1]
        
        #ops_i-=len(buffer)+1
    elif count_reps>2:                              #Example: "ababac"
        for elem in buffer:
            if 'off' in elem[0]:
                del elem[0]["off"]
                del elem[0]["size"]
            if 'mode' in elem[0]:
                del elem[0]['mode']
            group1['group'].append(elem[0])
        ops_grouped.append((group1,right_count)) 

    buffer.clear()
    i = 0

def make_graph():
    global ops, ops_grouped
    graph = pydot.Dot('graph_ops', graph_type='graph')
    counter = 0

    for op in ops_grouped:
        if op[0]['syscall']=="fault":
            graph.add_node(pydot.Node(counter, color = 'red', shape='invtrapezium', label = op[0]['syscall'] ))
        else:
            label = ''
            if op[0]['syscall'] == 'group':
                for syscall in op[0]['group']:
                    if syscall['syscall'] == 'rename':
                        label += syscall['syscall'] + ' from=' +  syscall['from'] + ' to=' +  syscall['to'] + '\n'
                    else:
                        label += syscall['syscall'] + ' path=' +  syscall['path'] + '\n'
            else:
                syscall = op[0]
                if syscall['syscall'] == 'rename':
                    label = syscall['syscall'] + ' from=' +  syscall['from'] + ' to=' +  syscall['to']
                else:
                    label = op[0]['syscall'] + ' path=' + op[0]['path']
                if 'size' in op[0]:
                    label += ' size=' + op[0]['size']
                if 'off' in op[0]:
                    label += ' off=' + op[0]['off']
                if 'mode' in op[0]:
                    label += ' mode=' + op[0]['mode']
            graph.add_node(pydot.Node(counter, shape='rect', label = label + '\nreps ' + str(op[1])))
        
        if counter<len(ops_grouped)-1:
            graph.add_edge(pydot.Edge(counter, counter+1, color='blue'))
            counter += 1
    graph.write_png('logs_graph.png')

def ouput_text():
    global ops, ops_grouped
    with open("parse_out_text.txt",'w') as f:
        for op in ops_grouped:
            if op[0]['syscall'] == 'fault':
                f.write('\n***************** FAULT ******************')
            else:     
                f.write("_____________________________ reps: " + str(op[1]) + " _____________________________\n")
                f.write(str(op[0]) + "\n")

def output_filter():
    global ops
    with open("parse_out.txt",'w') as f:
        for op in ops:  
            f.write(str(op) + '\n')


parser = argparse.ArgumentParser()

parser.add_argument("--logpath", nargs=1, help="Log file to parse.", dest='logpath')
parser.add_argument("--syscalls", nargs='*', help="System calls to consider. Examples: \"rename write\".", dest='syscalls')
parser.add_argument("-t","--text", action='store_true', help="The output will be in text format, not in a graph.")
parser.add_argument("--files", nargs='*', help="Only syscalls for those paths will be considered.", dest='files')
parser.add_argument("--limit_group", nargs=1, help="Define the maximum number of ops in a group. A lower number is better for performance.", dest='limit_group')
parser.add_argument("-fo","--filter_only", action='store_true',help="Define the maximum number of ops in a group. A lower number is better for performance.")

args = parser.parse_args()	

if args.logpath:
    log_file = args.logpath[0]
if args.limit_group:
    limit_group = args.limit_group[0]
if args.syscalls:
    covered_syscalls_arr = args.syscalls
    covered_syscalls = ''
    for syscall in covered_syscalls_arr[:-1]:
        covered_syscalls += syscall+'|'
    covered_syscalls += covered_syscalls_arr[-1]
if args.text:
    text = True
if args.filter_only:
    filter_only = True
if args.files:
    files_arr = args.paths
    files = ''
    for file in files_arr[:-1]:
        files += file+'|'
    files += files_arr[-1]


parse_ops()
if filter_only:
    output_filter()
else: 
    group_ops(ops)
    if text:
        ouput_text()
    else:
        make_graph()






