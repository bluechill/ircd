CC=g++
CXX=$(CC)
LD=$(CC)
AR=ar

CFLAGS=-g -fPIC
CXXFLAGS=$(CFLAGS)
LDFLAGS=-lpthread -ldl -lrt
ARFLAGS=-cr

IRCD_LIBRARY=IRCd.a
IRCD_LIBRARY_OBJECTS=Server.o IRC_Server.o Plugin.o IRC_Plugin.o Config.o
PLUGINS=plugins/names-plugin.plugin

OBJECTS=main.o

PROG=ircd

all: $(PROG) $(PLUGINS)

$(PROG): Makefile $(IRCD_LIBRARY) $(OBJECTS)
	@$(LD) $(LDFLAGS) -o $(PROG) $(OBJECTS) $(IRCD_LIBRARY)

$(IRCD_LIBRARY): Makefile $(IRCD_LIBRARY_OBJECTS)
	@$(AR) $(ARFLAGS) $(IRCD_LIBRARY) $(IRCD_LIBRARY_OBJECTS) 

%.o: %.cpp %.h Makefile
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

%.plugin: %.cpp Makefile $(IRCD_LIBRARY)
	@mkdir -p plugins
	@$(CXX) $(CXXFLAGS) -c -o $(<:.cpp=.o) $< -I./ -Wno-return-type-c-linkage
	@$(LD) $(LDFLAGS) -shared -o $@ $(<:.cpp=.o) $(IRCD_LIBRARY)

clean:
	@rm -f $(IRCD_LIBRARY)
	@rm -f $(IRCD_LIBRARY_OBJECTS)
	@rm -f $(OBJECTS)
	@rm -f $(PROG)
	@rm -f $(PLUGINS)
	@rm -f $(PLUGINS:.plugin=.o)
