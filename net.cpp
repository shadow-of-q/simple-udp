#include "sys.hpp"
#include "net.hpp"

namespace q {
namespace net {

typedef sockaddr_in address;

buffer::buffer(int len = 0, char *ptr = NULL) : len(len) {
  if (ptr && len != 0) memcpy(data, ptr, len);
}

socket::socket() : fd(::socket(PF_INET, SOCK_DGRAM, 0)) {
  const int reuseaddr = 1;
  const int rcvbuffersz = 256 * 1024;
  const int sndbuffersz = 256 * 1024;
#define TRY(EXP) if (EXP == -1) sys::fatal("net: unable to start socket");
  TRY (fcntl(fd, F_SETFL, O_NONBLOCK | fcntl(fd, F_GETFL)));
  TRY (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*) &reuseaddr, sizeof(int)));
  TRY (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*) &rcvbuffersz, sizeof(int)));
  TRY (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*) &sndbuffersz, sizeof(int)));
#undef TRY
}

void socket::bind(u16 port) {
  struct sockaddr_in servaddr;
  memset(&servaddr,0,sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
  servaddr.sin_port=htons(port);
  ::bind(fd, (sockaddr*) &servaddr, sizeof(servaddr));
}

int socket::receive(address &addr, buffer &buf) {
  socklen_t addrlen = sizeof(addr);
  const int flags = MSG_NOSIGNAL;
  const auto len = recvfrom(fd, buf.data, BUF_SZ, flags, (sockaddr*)&addr, &addrlen);
  if (len != -1) return len;
  if (errno != EWOULDBLOCK) sys::fatal("net: error receiving data on socket");
  return buf.len = 0;
}

int socket::send(const address &addr, const buffer &buf) {
  const socklen_t addrlen = sizeof(addr);
  const auto len = sendto(fd, buf.data, buf.len, MSG_NOSIGNAL, (const sockaddr*) &addr, addrlen);
  if (len != -1) return len;
  if (errno != EWOULDBLOCK) sys::fatal("net: error sending data on socket");
  return 0;
}

socket create_server(u16 port) {
  socket sock;
  sock.bind(port);
  return sock;
}

socket create_client() { return socket(); }

} /* namespace net */
} /* namespace q */

