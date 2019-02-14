# simple-pstree

## Overview
1. simple_pstree packages the option and PID into a message and sends it to ksimple_pstree via the netlink socket.
2. ksimple_pstree packages the tree of processes into messages and sends it to simple_pstree via the same socket.
3. simple_pstree displays the received message.

## Usage
`simple_pstree [-c|-s|-p][pid]`
* -c: Display the entire process tree which is spawned by a process.
* -s: Display all siblings of a process.
* -p: Display all ancestors of a process.

-c: The tree of processes is rooted at either pid or init if pid is omitted.(including the thread)

-s: The searching PID of the process can be either pid or simple_pstree if pid is omitted.(including the thread)

-p: The searching PID of the process can be either pid or simple_pstree if pid is omitted.

