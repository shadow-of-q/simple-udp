#CXX=clang++
#CXX=~/src/emscripten/em++
CXXOPTFLAGS=-Wall -Os -DNDEBUG -std=c++11
CXXDEBUGFLAGS=-Wall -O0 -g -std=c++11
CXXFLAGS=$(CXXDEBUGFLAGS) -I./
CLIENT_OBJS=client.o network.o net.o
SERVER_OBJS=server.o network.o net.o
all: client server
HEADERS=sys.hpp
client.o: client.cpp $(HEADERS)
server.o: server.cpp $(HEADERS)
sys.o: sys.cpp $(HEADERS)

client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o client $(CLIENT_OBJS)

server: server.o network.o
	$(CXX) $(CXXFLAGS) -o server $(SERVER_OBJS)

clean:
	-rm -f $(CLIENT_OBJS) $(SERVER_OBJS) client server

