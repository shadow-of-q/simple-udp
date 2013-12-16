#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#include "sys.hpp"
#include "net.hpp"

namespace q {
namespace sys {
void fatal(const char *s, const char *o) {
  sprintf_sd(m)("%s%s\n", s, o);
  exit(EXIT_FAILURE);
}
} /* namespace sys */
} /* namespace q */

namespace q {
namespace net {

address::address(u32 addr, u16 port) : ip(htonl(addr)), port(port){}
address::address(const char *addr, u16 port) :
  ip(inet_addr(addr)), port(htons(port)){}

buffer::buffer(u32 len, char *ptr) : len(len) {
  if (ptr && len != 0) memcpy(data, ptr, len);
}

socket::socket(u16 port) : fd(::socket(PF_INET, SOCK_DGRAM, 0)) {
  const int reuseaddr = 1;
  const int rcvbuffersz = 256 * 1024;
  const int sndbuffersz = 256 * 1024;
#define TRY(EXP) if (EXP == -1) sys::fatal("net: unable to start socket");
  TRY (fcntl(fd, F_SETFL, O_NONBLOCK | fcntl(fd, F_GETFL)));
  TRY (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*) &reuseaddr, sizeof(int)));
  TRY (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*) &rcvbuffersz, sizeof(int)));
  TRY (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*) &sndbuffersz, sizeof(int)));
#undef TRY
  if (port != 0) {
    struct sockaddr_in servaddr;
    memset(&servaddr,0,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    servaddr.sin_port=htons(port);
    bind(fd, (sockaddr*) &servaddr, sizeof(servaddr));
  }
}

INLINE sockaddr_in makeaddress(const address &addr) {
  sockaddr_in out;
  bzero(&out, sizeof(out));
  out.sin_family = AF_INET;
  out.sin_addr.s_addr = addr.ip;
  out.sin_port = addr.port;
  return out;
}

int socket::receive(address &addr, buffer &buf) const {
  const auto outaddr = makeaddress(addr);
  socklen_t addrlen = sizeof(outaddr);
  const int flags = MSG_NOSIGNAL;
  buf.len = recvfrom(fd, buf.data, BUF_SZ, flags, (sockaddr*)&outaddr, &addrlen);
  if (buf.len != u32(-1)) {
    addr.port = outaddr.sin_port;
    addr.ip = outaddr.sin_addr.s_addr;
    return buf.len;
  }
  if (errno != EWOULDBLOCK) sys::fatal("net: error receiving data on socket");
  return buf.len = 0;
}
int socket::receive(buffer &buf) const {
  const int flags = MSG_NOSIGNAL;
  buf.len = recvfrom(fd, buf.data, BUF_SZ, flags, NULL, NULL);
  if (buf.len != u32(-1)) return buf.len;
  if (errno != EWOULDBLOCK) sys::fatal("net: error receiving data on socket");
  return buf.len = 0;
}

int socket::send(const address &addr, const buffer &buf) const {
  const auto outaddr = makeaddress(addr);
  const socklen_t addrlen = sizeof(outaddr);
  const auto len = sendto(fd, buf.data, buf.len, MSG_NOSIGNAL, (const sockaddr*) &outaddr, addrlen);
  if (len != -1) return len;
  if (errno != EWOULDBLOCK) sys::fatal("net: error sending data on socket");
  return 0;
}
} /* namespace net */
} /* namespace q */

