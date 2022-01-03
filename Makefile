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
        mkdir ~/.config/rcon
        cp ./rcon.conf ~/.config/rcon
        chmod 660 ~/.config/rcon/rcon.conf

$(TARGET): $(SRC)
        gcc $(OPTIONS) $(SRC) $(INCLUDE) -o $(TARGET)
        strip --strip-all rcon
