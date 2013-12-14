#include <netinet/in.h>
#include "sys.hpp"

namespace q {
namespace net {

static const u32 MTU = 1400;
static const u32 BUF_SZ = MTU - 128;

// ipv4 address
typedef sockaddr_in address;

// buffers are fixed-size-allocated for maximum simplicity
struct buffer {
  buffer(int len = 0, char *ptr = NULL);
  int len;
  char data[BUF_SZ];
};

// very basic udp socket interface
struct socket {
  socket();
  void bind(u16 port);
  int receive(address &addr, buffer &buf);
  int send(const address &addr, const buffer &buf);
  int fd;
};

socket create_server(u16 port);
socket create_client();

} /* namespace net */
} /* namespace q */

