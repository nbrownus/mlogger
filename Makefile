all: mlogger

mlogger: mlogger.c
	gcc -Wall -Wextra -O3 -o mlogger mlogger.c

clean:
	rm mlogger
