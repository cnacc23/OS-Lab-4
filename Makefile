all: shell

shell:
	gcc -std=c99 -Wall -pedantic main.c scanner.c shell.c -o shell

clean:
	rm -f *~
	rm -f *.o
	rm -f shell
