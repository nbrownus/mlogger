A simple utility that logs different sources to syslog

is pretty much just bsdutils logger, which is shipped with many linux distros, Ubuntu being one.
It has been patched to provide support for log lines greater than 1k. Currently set to 64k.
The limit can be raised even further by changing the `MAX_LINE` constant.

You may also have to patch syslog.h for your kernel. This was not required on Ubuntu 12.04

### Building

To build mlogger

    make

### Usage

logger(1) man page is pretty useful, there is also the `-h` flag which prints the following

    Usage:
    mlogger [options] [message]

    Options:
     -d, --udp             use UDP (TCP is default)
     -i, --id              log the process ID too
     -f, --file <file>     log the contents of this file
     -h, --help            display this help text and exit
     -n, --server <name>   write to this remote syslog server
     -P, --port <number>   use this UDP port
     -p, --priority <prio> mark given message with this priority
     -s, --stderr          output message to standard error as well
     -t, --tag <tag>       mark every line with this tag
     -u, --socket <socket> write to this Unix socket
     -V, --version         output version information and exit
