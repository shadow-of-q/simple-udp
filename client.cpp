#include "sys.hpp"
#include "net.cpp"

int main(int argc, char**argv) {
  using namespace q;
  net::socket client;
  for (;;) {
    net::buffer buf;
    net::address addr("127.0.0.1", 1234);
    // net::address addr("2.25.198.185", 50000);
    buf.m_len = sprintf(buf.m_data, "coucou!")+1;
    client.send(addr, buf);
    sleep(2);
    if (client.receive(buf) == 0) continue;
    printf("from server: %s\n", buf.m_data);
  }
}

