CC      = cc
CXX     = c++

# Use the following CFLAGS and LIBS if you don't want to use gpsd.
CFLAGS  = -g -O3 -Wall -DHAVE_LOG_H -std=c++0x -pthread
LIBS    = -lpthread

# Use the following CFLAGS and LIBS if you do want to use gpsd.
#CFLAGS  = -g -O3 -Wall -DHAVE_LOG_H -DUSE_GPSD -std=c++0x -pthread
#LIBS    = -lpthread -lgps

LDFLAGS = -g

OBJECTS = Conf.o Echo.o Log.o M17Network.o Mutex.o M17Gateway.o M17Utils.o Reflectors.o RptNetwork.o StopWatch.o Thread.o Timer.o UDPSocket.o Utils.o Voice.o

all:		M17Gateway

M17Gateway:	$(OBJECTS)
		$(CXX) $(OBJECTS) $(CFLAGS) $(LIBS) -o M17Gateway

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<

install:
		install -m 755 M17Gateway /usr/local/bin/

clean:
		$(RM) M17Gateway *.o *.d *.bak *~
 
