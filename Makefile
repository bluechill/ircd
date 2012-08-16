CC=clang++
CXX=$(CC)
LD=$(CC)

OBJECTS=Server.o main.o

PROG=ircd

all: $(PROG)

$(PROG): Makefile $(OBJECTS)
	@$(LD) -o $(PROG) $(OBJECTS)

%.o: %.cpp
	@$(CXX) -c -o $@ $^

clean:
	@rm -f $(OBJECTS)
	@rm -f $(PROG)
