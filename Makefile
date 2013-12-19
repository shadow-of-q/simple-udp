#CXX=clang++
#CXX=~/src/emscripten/em++
CXXOPTFLAGS=-Wall -Os -DNDEBUG -std=c++11
CXXDEBUGFLAGS=-Wall -O0 -g -std=c++11
CXXFLAGS=$(CXXDEBUGFLAGS) -I./
CLIENT_OBJS=client.o
SERVER_OBJS=server.o
all: client server
HEADERS=sys.hpp net.hpp
client.o: client.cpp net.cpp $(HEADERS)
server.o: server.cpp net.cpp $(HEADERS)
sys.o: sys.cpp $(HEADERS)

client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o client $(CLIENT_OBJS)

server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o server $(SERVER_OBJS)

clean:
	-rm -f $(CLIENT_OBJS) $(SERVER_OBJS) client server

