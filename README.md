### mlogger

A simple utility that logs different sources to syslog

It is based on bsdutils `logger`, which is shipped with many linux distros, Ubuntu being one.

It has been patched for:

 - Log lines greater than 1k. Currently set to 64k. The limit can be raised even further by
   changing the `MAX_LINE` constant. You may also have to patch syslog.h for your kernel,
   this was not required on Ubuntu 12.04
 - Indented line continuation. If enabled any line that begins with a tab or a space following a line that did
   was not indented will be added to the previous line(s). There is a configurable timeout to keep the detection delay
   to a minimum. A few milliseconds seems to work well here
 - When logging to a UDP server (-n/--server), the size limit is still 400 characters. This limitation is inherit in UDP.
 - When logging to a Unix socket (-u/--socket), the size limit is also limited by the kernel maximum size which is commonly around 120KB ( see `/proc/sys/net/core/wmem_default` ).

### Usage

    mlogger [options] [message]

    Options:
     -d, --udp              use UDP (TCP is default)
     -i, --id               log the process ID too
     -f, --file <file>      log the contents of this file
     -h, --help             display this help text and exit
     -n, --server <name>    write to this remote syslog server
     -P, --port <number>    use this UDP port
     -p, --priority <prio>  mark given message with this priority
     -s, --stderr           output message to standard error as well
     -t, --tag <tag>        mark every line with this tag
     -u, --socket <socket>  write to this Unix socket
     -I, --indent <ms>      Will consider intended lines continuation of the previous line, within the timeout
                            specified in milliseconds
     -V, --version          output version information and exit

or

    man mlogger

### Building

To build mlogger

    make
