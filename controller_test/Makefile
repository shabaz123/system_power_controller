NAME = i2ctest
LIB_PATH = /usr/local/lib
OBJ = i2cfunc.o 
EXTENSION = .cpp
CC = g++
CFLAGS = -Wall -I/usr/local/include -g
LIBS = -lwiringPi

all: $(NAME)

i2ctest: i2ctest.cpp

%.o: %$(EXTENSION) $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(NAME): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -rf *.o $(NAME)


