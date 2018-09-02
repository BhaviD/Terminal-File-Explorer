CC = g++
CFLAGS = -Wall -std=c++1z
DEPS = includes.h common.h command_mode.h normal_mode.h
OBJ = common.o command_mode.o normal_mode.o
%.o: %.cpp $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

bhavi-file-explorer: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

