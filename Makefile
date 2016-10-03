all: mlogger

mlogger: mlogger.c
	gcc -Wall -Wextra -O3 -o mlogger mlogger.c

clean:
	rm -f mlogger
	rm -f docs/*.1

docs:
	/usr/local/bin/ronn -r docs/*.ronn

.PHONY: docs
