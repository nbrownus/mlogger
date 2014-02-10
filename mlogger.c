/*
 * Copyright (c) 1983, 1993
 *    The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the University of
 *    California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 1999-02-22 Arkadiusz Mi�kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 * Sun Mar 21 1999 - Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 * - fixed strerr(errno) in gettext calls
 * 2013-11-22 Nathan Brown <nbrown.us@gmail.com>
 * - Increased line length limit
 * - Divorced from bsdutils
 * - Cleaned out some cobwebs
 */

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <getopt.h>
#include <err.h>

#define SYSLOG_NAMES
#include <syslog.h>

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#define PROGRAM_NAME "mlogger"
#define PROGRAM_VERSION "1.1.0"
#define MAX_LINE 65536

int decode (char *, CODE *);
int pencode (char *);
long strtol_or_err (const char *str, const char *errmesg);
int readBlock (char *buf, int maxlen, long timeout_sec, long timeout_usec);

static int optd = 0;
static int udpport = 514;
static int dgram_size_limit = 400; /* original DGRAM size limit, based on minimal UDP datagram */

static int myopenlog (const char *sock) {
   int fd;
   static struct sockaddr_un s_addr; /* AF_UNIX address of local logger */
   socklen_t optlen;

   if (strlen(sock) >= sizeof(s_addr.sun_path)) {
       errx(EXIT_FAILURE, "openlog %s: pathname too long", sock);
   }

   s_addr.sun_family = AF_UNIX;
   (void) strcpy(s_addr.sun_path, sock);

   if ((fd = socket(AF_UNIX, optd ? SOCK_DGRAM : SOCK_STREAM, 0)) == -1) {
       err(EXIT_FAILURE, "socket %s", sock);
   }

   if (connect(fd, (struct sockaddr *) &s_addr, sizeof(s_addr)) == -1) {
       err(EXIT_FAILURE, "connect %s", sock);
   }

   optlen = sizeof(dgram_size_limit);
   if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF,
                  &dgram_size_limit, &optlen) == -1) {
      err(EXIT_FAILURE, "getsockopt %s", sock);
   }

   return fd;
}

static int udpopenlog (const char *servername, int port) {
    int fd;
    struct sockaddr_in s_addr;
    struct hostent *serverhost;

    if ((serverhost = gethostbyname(servername)) == NULL ) {
        errx(EXIT_FAILURE, "unable to resolve '%s'", servername);
    }

    if ((fd = socket(AF_INET, SOCK_DGRAM , 0)) == -1) {
        err(EXIT_FAILURE, "socket");
    }

    bcopy(serverhost->h_addr, &s_addr.sin_addr, serverhost->h_length);
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);

    if (connect(fd, (struct sockaddr *) &s_addr, sizeof(s_addr)) == -1) {
        err(EXIT_FAILURE, "connect");
    }

    return fd;
}


static void mysyslog (int fd, int logflags, int pri, char *tag, char *msg) {
    char buf[MAX_LINE], pid[30], *cp, *tp;
    time_t now;
    int prefix_len;
    int msg_len;

    if (fd > -1) {
        if (logflags & LOG_PID) {
            snprintf (pid, sizeof(pid), "[%d]", getpid());
        } else {
            pid[0] = 0;
        }

        if (tag) {
            cp = tag;
        } else {
            cp = getlogin();
            if (!cp) {
                cp = "<someone>";
            }
        }

        (void) time(&now);
        tp = ctime(&now) + 4;

        prefix_len = snprintf(buf, sizeof(buf), "<%d>%.15s %.200s%s: ", pri, tp, cp, pid);
        if (prefix_len>=(int)sizeof(buf)) {
            return; /* error: sizeof(buf) was too small, even for the prefix */
        }
        msg_len = MIN((int)sizeof(buf)-prefix_len-1, dgram_size_limit);
        strncat(buf,msg,msg_len);

        if (write(fd, buf, strlen(buf) + 1) < 0) {
            return; /* error */
        }
    }
}

static void usage (FILE *out) {
    fputs("\nUsage:\n", out);
    fputs("mlogger [options] [message]\n", out);

    fputs("\nOptions:\n", out);
    fputs(" -d, --udp              use UDP (TCP is default)\n"
          " -i, --id               log the process ID too\n"
          " -f, --file <file>      log the contents of this file\n"
          " -h, --help             display this help text and exit\n", out);
    fputs(" -n, --server <name>    write to this remote syslog server\n"
          " -P, --port <number>    use this UDP port\n"
          " -p, --priority <prio>  mark given message with this priority\n"
          " -s, --stderr           output message to standard error as well\n", out);
    fputs(" -t, --tag <tag>        mark every line with this tag\n"
          " -u, --socket <socket>  write to this Unix socket\n"
          " -I, --indent <ms>      Will consider intended lines continuation of the previous line, within the timeout\n"
          "                        specified in milliseconds\n"
          " -V, --version          output version information and exit\n\n", out);

    fprintf(out,"This mlogger is configured to send messages up to %d bytes.\n\n", MAX_LINE);

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

/*
 * logger -- read and log utility
 * Reads from an input and arranges to write the result on the system log.
 */
int main (int argc, char **argv) {
    int ch, logflags, pri;
    char *tag, buf[MAX_LINE];
    char *usock = NULL;
    long timeout_ms;
    long timeout_sec;
    long timeout_usec;
    int indent_mode;
    char *udpserver = NULL;
    int LogSock = -1;
    long tmpport;

    static const struct option longopts[] = {
        { "id",       no_argument,        0, 'i' },
        { "stderr",   no_argument,        0, 's' },
        { "file",     required_argument,  0, 'f' },
        { "priority", required_argument,  0, 'p' },
        { "tag",      required_argument,  0, 't' },
        { "socket",   required_argument,  0, 'u' },
        { "udp",      no_argument,        0, 'd' },
        { "server",   required_argument,  0, 'n' },
        { "port",     required_argument,  0, 'P' },
        { "indent",   required_argument,  0, 'I' },
        { "version",  no_argument,        0, 'V' },
        { "help",     no_argument,        0, 'h' },
        { NULL,       0,                  0, 0   }
    };

    tag = NULL;
    pri = LOG_NOTICE;
    logflags = 0;

    while ((ch = getopt_long(argc, argv, "f:ip:st:u:dI:n:P:Vh", longopts, NULL)) != -1) {
        switch((char) ch) {
            case 'f': /* file to log */
                if (freopen(optarg, "r", stdin) == NULL)
                    err(EXIT_FAILURE, "file %s", optarg);
                break;

            case 'i': /* log process id also */
                logflags |= LOG_PID;
                break;

            case 'p': /* priority */
                pri = pencode(optarg);
                break;

            case 's': /* log to standard error */
                logflags |= LOG_PERROR;
                break;

            case 't': /* tag */
                tag = optarg;
                break;

            case 'u': /* unix socket */
                usock = optarg;
                break;

            case 'd':
                optd = 1; /* use datagrams */
                break;

            case 'n': /* udp socket */
                optd = 1; /* use datagrams because udp */
                udpserver = optarg;
                break;

            case 'P': /* change udp port */
                tmpport = strtol_or_err(optarg, "failed to parse port number");

                if (tmpport < 0 || 65535 < tmpport) {
                    errx(EXIT_FAILURE, "port `%ld' out of range", tmpport);
                }

                udpport = (int) tmpport;
                break;

            case 'V':
                printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
                exit(EXIT_SUCCESS);

            case 'I':
                indent_mode = 1;
                timeout_ms = atol(optarg);

                if (timeout_ms < 1) {
                    errx(EXIT_FAILURE, "Invalid value for timeout %li", timeout_ms);
                }

                break;

            case 'h':
                usage(stdout);

            case '?':
            default:
                usage(stderr);
        }
    }

    argc -= optind;
    argv += optind;

    /* setup for logging */
    if (!usock && !udpserver) {
        openlog(tag ? tag : getlogin(), logflags, 0);
    } else if (udpserver) {
        LogSock = udpopenlog(udpserver, udpport);
    } else {
        LogSock = myopenlog(usock);
    }

    (void) fclose(stdout);

    /* log input line if appropriate */
    if (argc > 0) {
        char *p, *endp;
        size_t len;

        for (p = buf, endp = buf + sizeof(buf) - 2; *argv;) {
            len = strlen(*argv);
            if (p + len > endp && p > buf) {
                if (!usock && !udpserver) {
                    syslog(pri, "%s", buf);
                } else {
                    mysyslog(LogSock, logflags, pri, tag, buf);
                }

                p = buf;
            }

            if (len > sizeof(buf) - 1) {
                if (!usock && !udpserver) {
                    syslog(pri, "%s", *argv++);
                } else {
                    mysyslog(LogSock, logflags, pri, tag, *argv++);
                }

            } else {
                if (p != buf) {
                    *p++ = ' ';
                }

                memmove(p, *argv++, len);
                *(p += len) = '\0';
            }
        }

        if (p != buf) {
            if (!usock && !udpserver) {
                syslog(pri, "%s", buf);
            } else {
                mysyslog(LogSock, logflags, pri, tag, buf);
            }
        }

    } else if (indent_mode) {
        int len;

        timeout_sec = timeout_ms / 1000;
        timeout_usec = (timeout_ms % 1000) * 1000;

        while ((len = readBlock(buf, MAX_LINE, timeout_sec, timeout_usec)) != EOF) {
            //fprintf(stderr, "Got buf %i\n", len);
            if (len > 0 && buf[len - 1] == '\n') {
                buf[len - 1] = '\0';
            }

            if (!usock && !udpserver) {
                syslog(pri, "%s", buf);
            } else {
                mysyslog(LogSock, logflags, pri, tag, buf);
            }
        }
    } else {
        while (fgets(buf, sizeof(buf), stdin) != NULL) {
            /* glibc is buggy and adds an additional newline, so we have to remove it here until glibc is fixed */
            int len = strlen(buf);

            if (len > 0 && buf[len - 1] == '\n') {
                buf[len - 1] = '\0';
            }

            if (!usock && !udpserver) {
                syslog(pri, "%s", buf);
            } else {
                mysyslog(LogSock, logflags, pri, tag, buf);
            }
        }
    }

    if (!usock && !udpserver) {
        closelog();
    } else {
        close(LogSock);
    }

    return EXIT_SUCCESS;
}

/*
 * Reads blocks of text from standard in
 * This will attempt to keep things like stack traces together in a single "log line"
 * Any line that starts with a tab or space will be considered part of the previous line, if there was one
 * If no continuation lines were received within the current line is considered complete
 * This will block until there is at least 1 line, the maximum amount of time to receive a line is the timeout
 */
int readBlock (char *buf, int maxlen, long timeout_sec, long timeout_usec) {
    fd_set set;
    struct timeval timeout;
    int retval;
    int offset = 0;
    int nextc;

    while (1) {
        FD_ZERO(&set);
        FD_SET(fileno(stdin), &set);

        timeout.tv_sec = timeout_sec;
        timeout.tv_usec = timeout_usec;

        retval = select(fileno(stdin) + 1, &set, NULL, NULL, &timeout);

        if (retval == -1) {
            goto err;
        } else if (retval == 0) {
            //We have something to return and the poll had nothing to give us
            if (offset > 0) {
                return offset;
            }
        }

        if (offset > 0) {
            if ((nextc = fgetc(stdin)) == EOF) {
                goto err;
            }

            ungetc(nextc, stdin);
            if (nextc != '\t' && nextc != ' ') {
                return offset;
            }
        }

        //We may sit here if we have nothing in the buffer already
        if (fgets(buf + offset, maxlen - offset, stdin) == NULL) {
            goto err;
        }

        offset = strlen(buf);
    }

err:
    if (offset > 0) {
        return offset;
    }

    return EOF;
}

/*
 *  Decode a symbolic name to a numeric value
 */
int pencode (char *s) {
    char *save;
    int fac, lev;

    for (save = s; *s && *s != '.'; ++s);

    if (*s) {
        *s = '\0';
        fac = decode(save, facilitynames);
        if (fac < 0)
            errx(EXIT_FAILURE, "unknown facility name: %s.", save);
        *s++ = '.';
    } else {
        fac = LOG_USER;
        s = save;
    }

    lev = decode(s, prioritynames);
    if (lev < 0) {
        errx(EXIT_FAILURE, "unknown priority name: %s.", save);
    }

    return ((lev & LOG_PRIMASK) | (fac & LOG_FACMASK));
}

int decode (char *name, CODE *codetab) {
    CODE *c;

    if (isdigit(*name)) {
        return (atoi(name));
    }

    for (c = codetab; c->c_name; c++) {
        if (!strcasecmp(name, c->c_name)) {
            return (c->c_val);
        }
    }
    return (-1);
}

/*
 * same as strtol(3) but exit on failure instead of returning crap
 * Copied from strutil.c in bsdutils
 */
long strtol_or_err (const char *str, const char *errmesg) {
    long num;
    char *end = NULL;

    if (str == NULL || *str == '\0') {
        goto err;
    }

    errno = 0;
    num = strtol(str, &end, 10);

    if (errno || str == end || (end && *end)) {
        goto err;
    }

    return num;

err:
    if (errno) {
        err(EXIT_FAILURE, "%s: '%s'", errmesg, str);
    } else {
        errx(EXIT_FAILURE, "%s: '%s'", errmesg, str);
    }

    return 0;
}
