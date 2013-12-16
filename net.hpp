#include <netinet/in.h>
#include "sys.hpp"

namespace q {
namespace net {

static const u32 MTU = 1400;
static const u32 BUF_SZ = MTU - 128;

// ipv4 address
struct address {
  INLINE address() {}
  address(u32 addr, u16 port);
  address(const char *addr, u16 port);
  u32 ip;
  u16 port;
};

// buffers are fixed-size-allocated for maximum simplicity
struct buffer {
  buffer(u32 len = 0, char *ptr = NULL);
  u32 len;
  char data[BUF_SZ];
};

// very basic udp socket interface
struct socket {
  socket(u16 port = 0); // port == 0 means client (no bind operation)
  int receive(buffer &buf) const;
  int receive(address &addr, buffer &buf) const;
  int send(const address &addr, const buffer &buf) const;
  int fd;
};
} /* namespace net */
} /* namespace q */

