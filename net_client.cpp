#include "net.hpp"
#include "net_protocol.hpp"
#include <unistd.h>

int main(int argc, const char *argv[]) {
  using namespace q;
  net::start();
  const net::address addr("127.0.0.1", 1234);
  const auto chan = net::channel::create(addr, 50000);
  chan->send(false, "csssss", SHORT5, 4, 3, 2, 1, 0);
  chan->flush();
  chan->send(false, "cii", INT2, 0, 1);
  chan->flush();
#if 1
  chan->send(false, "cii", INT2, 0, 1);
  chan->send(false, "cccc", CHAR3, 0, 1, 2);
  chan->send(false, "csssss", SHORT5, 0, 1, 2, 3, 4);
  chan->flush();
  chan->send(true, "csssss", SHORT5, 4, 3, 2, 1, 0);
  chan->flush();
  chan->send(true, "cii", INT2, 66, 6);
  chan->flush();
  chan->send(false, "c", RESEND);
  chan->flush();
  sleep(5);
  char op = 0;
  const auto len = chan->rcv("c", &op);
  assert(len == 1 && op == OK);
  chan->send(false, "cccc", CHAR3, 3, 3, 3);
  chan->flush();
#endif
  net::channel::destroy(chan);
  net::end();
  return 0;
}

