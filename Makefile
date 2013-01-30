UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
CC=g++
endif

ifeq ($(UNAME), Darwin)
CC=clang++
endif

CXX=$(CC)
LD=$(CC)
AR=ar

CFLAGS=-g -fPIC -ISSL/include -Wall
CXXFLAGS=$(CFLAGS)
LDFLAGS=-lpthread -ldl -lssl -lcrypto

ifeq ($(UNAME), Linux)
OpenSSLDIR = SSL/linux
LDFLAGS += -lrt
else ifeq ($(UNAME), FreeBSD)
OpenSSLDIR = SSL/freebsd
else ifeq ($(UNAME), Darwin)
OpenSSLDIR = SSL/darwin
else
OpenSSLDIR = SSL/unknown
endif

LDFLAGS += -L$(OpenSSLDIR)

ARFLAGS=-cr

IRCD_LIBRARY=IRCd.a
IRCD_LIBRARY_OBJECTS=Server.o IRC_Server.o Plugin.o IRC_Plugin.o IRC_Service.o Config.o
PLUGINS=plugins/names-plugin.plugin plugins/join-plugin.plugin plugins/quit-plugin.plugin plugins/list-plugin.plugin plugins/part-plugin.plugin plugins/privmsg-plugin.plugin plugins/user-plugin.plugin plugins/pong-plugin.plugin plugins/nick-plugin.plugin plugins/mode-plugin.plugin plugins/oper-plugin.plugin plugins/conreg-plugin.plugin plugins/topic-plugin.plugin plugins/ison-plugin.plugin plugins/example-service.service

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

%.service: %.cpp Makefile $(IRCD_LIBRARY)
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
	@rm -f $(PLUGINS:.service=.o)

distclean:
	@make clean
	@cd OpenSSL && make clean && cd ..
	@find . -name "*.o" -exec rm -f {} \;
	@rm -rf ssl-install
	@rm -rf SSL
