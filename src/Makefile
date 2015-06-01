DEBUG=1

BUILD_VERSION ?= "\"7.0.0\""
VERSION=$(if $(BUILD_VERSION),-DVER_PRODUCTVERSION_STR=$(BUILD_VERSION))
CFLAGS += $(if $(DEBUG),-g -O0 -DDEBUG,-O2) $(VERSION) -D_LIN_ -Wall -c -I /usr/include/prlsdk/
LDFLAGS += $(if $(DEBUG),-g  -rdynamic,) -lprl_sdk

OBJS = \
	Utils.o \
	GetOpt.o \
	CmdParam.o \
	Logger.o \
	PrlXml.o \
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
	install -d $(DESTDIR)/usr/sbin
	install -m 755 $(prlctl_BINARY) $(DESTDIR)/usr/sbin
	install -m 755 $(prlsrvctl_BINARY) $(DESTDIR)/usr/sbin

clean:
	rm -f *.o $(prlctl_BINARY) $(prlsrvctl_BINARY) depend

.PHONY: all install clean depend
