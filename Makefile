CC=clang++
CXX=$(CC)
LD=$(CC)

CFLAGS=-g
CXXFLAGS=$(CFLAGS)
LDFLAGS=-lpthread

OBJECTS=Server.o IRC_Server.o Plugin.o main.o

PROG=ircd

all: $(PROG)

$(PROG): Makefile $(OBJECTS)
	@$(LD) $(LDFLAGS) -o $(PROG) $(OBJECTS)

%.o: %.cpp %.h
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	@rm -f $(OBJECTS)
	@rm -f $(PROG)
