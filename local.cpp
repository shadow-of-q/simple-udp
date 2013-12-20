#include "net.hpp"
#include "net_protocol.hpp"
#include <unistd.h>

int main() {
  using namespace q;
  net::start();
  const auto server = net::server::create(net::local);
  const auto client = net::channel::create(net::local);
  short s[5];
  char op;

  // basic test c2s
  client->send(false, "csssss", SHORT5, 4, 3, 2, 1, 0);
  client->flush();
  const auto chan = server->active();
  assert(chan != NULL);
  chan->rcv("csssss", &op, s, s+1, s+2, s+3, s+4);
  printf("%c %d %d %d %d %d\n", op, s[0], s[1], s[2], s[3], s[4]);
  assert(chan->rcv("c", &op) == 0);

  // basic test s2c
  chan->send(false, "csssss", SHORT5, 0, 1, 2, 3, 4);
  chan->flush();
  client->rcv("csssss", &op, s, s+1, s+2, s+3, s+4);
  printf("%c %d %d %d %d %d\n", op, s[0], s[1], s[2], s[3], s[4]);
  assert(chan->rcv("c", &op) == 0);

  // test overflow
  u32 sum = 0;
  loopi(257*1024) {
    chan->send(false, "i", i);
    chan->flush();
    sum += i;
  }

  u32 testsum = 0;
  loopi(257*1024) {
    u32 u;
    u32 len = client->rcv("i", &u);
    assert(len);
    testsum += u;
  }
  assert(testsum == sum);
  assert(chan->rcv("i", NULL) == 0);

  net::channel::destroy(chan);
  net::channel::destroy(client);
  net::server::destroy(server);
  net::end();
  return 0;
}

