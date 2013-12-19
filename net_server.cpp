#include "net.hpp"
#include <unistd.h>

int main(int argc, const char *argv[]) {
  using namespace q;
  net::start();
  const auto server = net::server::create(16, 50000, 1234);
  net::channel *chan = NULL;
  while ((chan = server->active()) == NULL) sleep(1);
  int data[2];
  chan->rcv("ii", data, data+1);
  chan->flush();
  printf("[%d %d]\n", data[0], data[1]);
  net::channel::destroy(chan);
  net::server::destroy(server);
  net::end();
  return 0;
}

