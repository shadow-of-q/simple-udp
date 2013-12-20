#include "net.hpp"
#include "net_protocol.hpp"
#include <unistd.h>

int main(int argc, const char *argv[]) {
  using namespace q;
  net::start();
  const auto server = net::server::create(16, 500000, 1234);
  vector<net::channel*> channels;
  printf("\n");
  for (;;) {
    net::channel *chan;
    while ((chan = server->active()) == NULL) sleep(1);
    printf("NEW MESSAGE\n");

    // should be the same channel as before
    if (channels.size() != 0)
      assert(channels[0] == chan);
    else
      channels.add(chan);

    char op;
    while (chan->rcv("c", &op) != 0) {
      switch (op) {
        case INT2: {
          int data[2];
          chan->rcv("ii", data+0, data+1);
          printf("[%d %d]\n", data[0], data[1]);
        }
        break;
        case CHAR3: {
          char data[3];
          chan->rcv("ccc", data+0, data+1, data+2);
          printf("[%d %d %d]\n", u32(data[0]), u32(data[1]), u32(data[2]));
        }
        break;
        case SHORT5: {
          short data[5];
          chan->rcv("sssss", data+0, data+1, data+2, data+3, data+4);
          printf("[%d %d %d %d %d]\n", u32(data[0]), u32(data[1]), u32(data[2]), u32(data[3]), u32(data[4]));
        }
        break;
        case RESEND: {
          chan->send(false, "c", OK);
          chan->flush();
        }
        break;
      }
    }
    fflush(stdout);
  }

  net::channel::destroy(channels[0]);
  net::server::destroy(server);
  net::end();
  return 0;
}

