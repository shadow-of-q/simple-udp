#CXX=clang++
#CXX=~/src/emscripten/em++
CXXOPTFLAGS=-Wall -Os -DNDEBUG -std=c++11
CXXDEBUGFLAGS=-Wall -O0 -g -std=c++11
CXXFLAGS=$(CXXDEBUGFLAGS) -I./
CLIENT_OBJS=client.o
SERVER_OBJS=server.o
NET_CLIENT_OBJS=net_client.o net.o
NET_SERVER_OBJS=net_server.o net.o
all: client server net_client net_server
HEADERS=sys.hpp net.hpp
client.o: client.cpp net.cpp $(HEADERS)
server.o: server.cpp net.cpp $(HEADERS)
net_client.o: net_client.cpp $(HEADERS)
net_server.o: net_server.cpp $(HEADERS)
sys.o: sys.cpp $(HEADERS)

client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o client $(CLIENT_OBJS)

server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o server $(SERVER_OBJS)

net_client: $(NET_CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o net_client $(NET_CLIENT_OBJS)

net_server: $(NET_SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o net_server $(NET_SERVER_OBJS)

clean:
	-rm -f $(NET_CLIENT_OBJS) $(NET_SERVER_OBJS) $(CLIENT_OBJS) $(SERVER_OBJS) \
	client server net_client net_server

