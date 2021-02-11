DEBUG=1

VERSION ?= $(shell cat ../Makefile.version)
CFLAGS += -DVERSION=\"$(VERSION)\"
CFLAGS += $(if $(DEBUG),-g -O0 -DDEBUG,-O2) -std=c++11 -D_LIN_ -Wall -c \
	-I/usr/include/prlsdk -I/usr/include/prlcommon
LDFLAGS += $(if $(DEBUG),-g  -rdynamic,) -lprl_sdk -lprlcommon -lpthread

OBJS = \
	Utils.o \
	GetOpt.o \
	CmdParam.o \
	Logger.o \
	PrlSignal.o \
	PrlCleanup.o \
	PrlOutFormatter.o \
	EventSyncObject_unix.o \
	PrlSharedFolder.o \
	PrlSnapshot.o \
	PrlDev.o \
	PrlBackup.o \
	PrlList.o	\
	PrlStat.o \
	PrlVm.o \
	PrlSrv.o \
	PrlDisp.o

prlctl_BINARY=prlctl
prlctl_OBJS= \
	PrlCtl.o	\
	PrlEnter.o

prlsrvctl_BINARY=prlsrvctl
prlsrvctl_OBJS= \
	PrlSrvCtl.o

all: depend $(prlctl_BINARY) $(prlsrvctl_BINARY)

$(prlctl_BINARY): $(prlctl_OBJS) $(OBJS)
	g++ -o $@ $(prlctl_OBJS) $(OBJS) $(LDFLAGS)

$(prlsrvctl_BINARY): $(prlsrvctl_OBJS) $(OBJS)
	g++ -o $@ $(prlsrvctl_OBJS) $(OBJS) $(LDFLAGS)

%.o: %.cpp
	g++ -c $(CFLAGS) -o $@ $<

depend:
	g++ $(CFLAGS) -M $(OBJS:.o=.cpp) -M $(prlctl_OBJS:.o=.cpp) -M $(prlsrvctl_OBJS:.o=.cpp) > depend

install:
	install -d $(DESTDIR)/usr/bin
	install -m 755 $(prlctl_BINARY) $(DESTDIR)/usr/bin
	install -m 755 $(prlsrvctl_BINARY) $(DESTDIR)/usr/bin

clean:
	rm -f *.o $(prlctl_BINARY) $(prlsrvctl_BINARY) depend

.PHONY: all install clean depend
