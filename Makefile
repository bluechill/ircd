CC=clang++
CXX=$(CC)
LD=$(CC)
AR=ar

CFLAGS=-g
CXXFLAGS=$(CFLAGS)
LDFLAGS=-lpthread
ARFLAGS=-cr

IRCD_LIBRARY=IRCd.a
IRCD_LIBRARY_OBJECTS=Server.o IRC_Server.o Plugin.o IRC_Plugin.o Config.o

OBJECTS=main.o

PROG=ircd

all: $(PROG)

$(PROG): Makefile $(IRCD_LIBRARY) $(OBJECTS)
	@$(LD) $(LDFLAGS) -o $(PROG) $(OBJECTS) $(IRCD_LIBRARY)

$(IRCD_LIBRARY): Makefile $(IRCD_LIBRARY_OBJECTS)
	@$(AR) $(ARFLAGS) $(IRCD_LIBRARY) $(IRCD_LIBRARY_OBJECTS) 

%.o: %.cpp %.h Makefile
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	@rm -f $(IRCD_LIBRARY)
	@rm -f $(IRCD_LIBRARY_OBJECTS)
	@rm -f $(OBJECTS)
	@rm -f $(PROG)
