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
  u32 m_ip;
  u16 m_port;
};

// buffers are fixed-size-allocated for maximum simplicity
struct buffer {
  buffer(void) : len(0) {}
  u32 len;
  char data[BUF_SZ];
};

// very basic udp socket interface
struct socket {
  socket(u16 port = 0); // port == 0 means client (no bind operation)
  ~socket();
  int receive(buffer &buf) const;
  int receive(address &addr, buffer &buf) const;
  int send(const address &addr, const buffer &buf) const;
  int fd;
};

struct channel {
  static channel *create(const address &addr);
  static void destroy(channel*);
  void disconnect();
  void send(bool reliable, const char *fmt, ...);
  int rcv(const char *fmt, ...);
  void flush();
};

struct server {
  static server *create(u16 port);
  static void destroy(server*);
  channel *activechannel();
};

} /* namespace net */
} /* namespace q */

