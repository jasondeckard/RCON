SRC = main.c rcon.c
INCLUDE = -I ./include
MAKEFLAGS += --silent
OPTIONS = -Wall
TARGET = rcon

all: $(TARGET)

clean:
	rm -f *.o $(TARGET)

debug:
	gcc -g $(OPTIONS) $(SRC) $(INCLUDE) -o $(TARGET)

install: all
	cp rcon ~/bin
	chmod 775 ~/bin/rcon
	cp ./.rcon ~
	chmod 660 ~/.rcon

$(TARGET): $(SRC)
	gcc $(OPTIONS) $(SRC) $(INCLUDE) -o $(TARGET)
	strip --strip-all rcon
