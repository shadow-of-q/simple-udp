#CXX=clang++
#CXX=~/src/emscripten/em++
CXXOPTFLAGS=-Wall -Os -DNDEBUG -std=c++11
CXXDEBUGFLAGS=-Wall -O0 -g -std=c++11
CXXFLAGS=$(CXXDEBUGFLAGS) -I./
all: client server net_client net_server too_large ping_server ping_client

HEADERS=sys.hpp net.hpp net_protocol.hpp
client.o: client.cpp net.cpp $(HEADERS)
server.o: server.cpp net.cpp $(HEADERS)
too_large.o: too_large.cpp $(HEADERS)
net_client.o: net_client.cpp $(HEADERS)
net_server.o: net_server.cpp $(HEADERS)
sys.o: sys.cpp $(HEADERS)
ping_server.o: ping_server.cpp $(HEADERS)
ping_client.o: ping_client.cpp $(HEADERS)

TOO_LARGE_OBJS=too_large.o net.o
CLIENT_OBJS=client.o
SERVER_OBJS=server.o
NET_CLIENT_OBJS=net_client.o net.o
NET_SERVER_OBJS=net_server.o net.o
PING_SERVER_OBJS=ping_server.o net.o
PING_CLIENT_OBJS=ping_client.o net.o

client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o client $(CLIENT_OBJS)

server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o server $(SERVER_OBJS)

net_client: $(NET_CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o net_client $(NET_CLIENT_OBJS)

net_server: $(NET_SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o net_server $(NET_SERVER_OBJS)

ping_client: $(PING_CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o ping_client $(PING_CLIENT_OBJS)

ping_server: $(PING_SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o ping_server $(PING_SERVER_OBJS)

too_large: $(TOO_LARGE_OBJS)
	$(CXX) $(CXXFLAGS) -o too_large $(TOO_LARGE_OBJS)

clean:
	-rm -f $(NET_CLIENT_OBJS) $(NET_SERVER_OBJS) \
				 $(PING_CLIENT_OBJS) $(PING_SERVER_OBJS) \
				 $(CLIENT_OBJS) $(SERVER_OBJS) \
	client server net_client net_server too_large ping_server ping_client

