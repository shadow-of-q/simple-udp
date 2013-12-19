#include "net.hpp"

int main(int argc, const char *argv[]) {
  using namespace q;
  net::start();
  const net::address addr("127.0.0.1", 1234);
  const auto chan = net::channel::create(addr, 5000);
  chan->send(false, "ii", 0, 1);
  chan->flush();
  net::channel::destroy(chan);
  net::end();
  return 0;
}

