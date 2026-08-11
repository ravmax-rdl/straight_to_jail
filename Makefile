# Straight to Jail -- build file.
#
# The canonical target reproduces spec section 4's mandated build line exactly:
#     gcc *.c -o monopoly
# Nothing here may ever be *required* to build the program.

CC     = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic
SRC    = $(wildcard *.c)

monopoly: $(SRC)
	$(CC) *.c -o monopoly

# Same sources, warnings on. This is the build used during development.
straight_to_jail: $(SRC)
	$(CC) $(CFLAGS) *.c -o straight_to_jail

# Enables the #ifdef DEBUG invariant assertions.
debug: $(SRC)
	$(CC) $(CFLAGS) -g -DDEBUG *.c -o monopoly

clean:
	rm -f monopoly monopoly.exe straight_to_jail straight_to_jail.exe

.PHONY: debug clean
