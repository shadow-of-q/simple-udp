#include "sys.hpp"
#include "net.hpp"

#include "unistd.h"
int main(int argc, char**argv) {
  using namespace q;
  if (argc != 2) {
    printf("usage:  udpcli <IP address>\n");
    exit(1);
  }
  net::socket client;
  for (;;) {
    net::buffer buf;
    net::address addr("127.0.0.1", 1234);
    buf.len = sprintf(buf.data, "coucou!")+1;
    client.send(addr, buf);
    sleep(2);
    if (client.receive(buf) == 0) continue;
    printf("from server: %s\n", buf.data);
  }
}

