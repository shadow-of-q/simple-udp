#include "net.hpp"
#include "net_protocol.hpp"
#include <unistd.h>

int main(int argc, const char *argv[]) {
  using namespace q;
  net::start();
  const auto server = net::server::create(16, 500000, 1234);
  printf("\n");
  net::channel *chan = NULL;
  for (;;) {
    while ((chan = server->active()) == NULL);
    chan->send(false, "%i", 0);
  }

  if (chan) net::channel::destroy(chan);
  net::server::destroy(server);
  net::end();
  return 0;
}

