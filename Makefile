all:
	gcc -v -Wall -Wpadded -Wextra -ansi -g -O2 basex.c -o basex
clean:
	rm basex
