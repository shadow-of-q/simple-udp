#include "sys.hpp"
#include "net.hpp"

int main(int argc, char**argv) {
  using namespace q;
  net::socket server(1234);
  for (;;) {
    net::buffer buf;
    net::address addr;
    if (server.receive(addr, buf) == 0) continue;
    printf("I am server\n");
    printf("from client: %s\n", buf.data);
    buf.len = sprintf(buf.data, "This is OK!\n")+1;
    server.send(addr, buf);
  }
  return 0;
}

