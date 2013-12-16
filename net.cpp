/*-------------------------------------------------------------------------
 - mini.q - a minimalistic multiplayer FPS
 - net.cpp -> implements a simple reliable UDP interface
 -------------------------------------------------------------------------*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
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

/*-------------------------------------------------------------------------
 - unix boiler plate
 -------------------------------------------------------------------------*/
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

socket::~socket() { if (fd != -1) close(fd); }

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

/*-------------------------------------------------------------------------
 - implements network protocol
 -------------------------------------------------------------------------*/
// we do not store more than 128 reliable *outstanding* reliable messages:
// beyond that, we just consider that the channel timed out. the user will be
// responsible to destroy it
static const u32 MAX_RELIABLE_NUM = 128;

struct internal_msg {
  static internal_msg *create(const buffer&);
  static void destroy(internal_msg*);
  u32 seq_reliable;   // 31 bits for sequence. 1 bit to know if reliable or not
  u32 ackseq_receipt; // 31 bits to acknowledge sequence. 1 bit for reliable ack
  u16 qport;          // handles port renaming by routers
  char data[];        // message itself
};

struct internal_channel {
  internal_channel(u32 ip, u16 port, struct internal_channel *owner = NULL);
  ~internal_channel();
  ringbuffer<internal_msg*, MAX_RELIABLE_NUM> outgoing_reliable;
  struct internal_server *owner;
  struct internal_msg *incoming;
  u32 ip;          // ip of the remote machine
  u16 qport;       // q(uake) port of the remote machine to fight port renaming by routers
  u16 timedout;    // if true, we do not handle this machine anymore
  u32 last_seq:31; // the larger sequence number we got from this remote machine
  u32 ack_rel:1;   //
};

struct internal_server {
  internal_server(u32 maxclients, u16 port);
  void remove(internal_channel*);
  internal_channel *active();
  internal_channel *timedout();
  internal_channel *findchannel(u32 ip, u16 qport) const;
  vector<internal_channel*> channels;
  socket sock;
  u32 maxclients;
};

internal_msg *internal_msg::create(const buffer &buf) {
  auto msg = (internal_msg *) malloc(buf.len);
  memcpy(msg, buf.data, buf.len);
  return msg;
}
void internal_msg::destroy(internal_msg *msg) { free(msg); }

internal_channel::internal_channel(u32 ip, u16 port, internal_channel *owner) :
  owner(NULL), incoming(NULL), ip(ip), qport(qport), timedout(0) {}

internal_channel::~internal_channel() {
  if (incoming != NULL) internal_msg::destroy(incoming);
  if (owner != NULL) owner->remove(this);
}

internal_server::internal_server(u32 maxclients, u16 port) :
  sock(port), maxclients(maxclients) {}

internal_channel *internal_server::findchannel(u32 ip, u16 qport) const {
  for (auto c : channels) if (c->ip==ip && c->qport==qport) return c;
  return NULL;
}

internal_channel *internal_server::active() {
  buffer buf;
  address addr;
  for (;;) {
    const int len = sock.receive(addr, buf);
    if (len == -1) break;
    const auto msg = (internal_msg*) buf.data;

    // keep it simple. allocate a new channel if not already here
    internal_channel *c;
    if ((c = findchannel(addr.ip, msg->qport)) == NULL) {
      // if we have too many clients, we have to drop this one
      if (u32(channels.size()) == maxclients) continue;
      c = channels.add(new internal_channel(addr.ip, msg->qport));
    }
    // do not deal with timed out channels. it is too late for them
    if (c->timedout) continue;
    c->incoming = internal_msg::create(buf);
    return c;
  }
  return NULL;
}

internal_channel *internal_server::timedout() {
  for (auto c : channels) if (c->timedout) return c;
  return NULL;
}

void internal_server::remove(internal_channel *chan) {
  u32 idx = 0, n = u32(channels.size());
  for (; idx < n; ++idx) if (chan == channels[idx]) break;
  if (idx == n) return;
  channels[idx] = channels[n-1];
  channels.setsize(n-1);
}

} /* namespace net */
} /* namespace q */

