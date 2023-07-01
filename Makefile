linux-shell.o: linux-shell.c
	gcc linux-shell.c -c linux-shell

linux-shell: linux-shell.o
	gcc linux-shell.o -o linux-shell

clean:
	rm -f linux-shell.o linux-shell

.PHONY: clean