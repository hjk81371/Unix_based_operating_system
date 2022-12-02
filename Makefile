compile:
	gcc -Wall -pedantic-errors -o shell shell.c
run: compile
	./shell
clean:
	rm shell
