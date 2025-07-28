CC = mpicc
CFLAGS = -Wall -std=c99 -g
LDFLAGS = -lm

SOURCES = main.c List.c DFS.c ST.c Tree.c Book.c
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = mpi_program

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(EXECUTABLE) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)