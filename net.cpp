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
#include <time.h>

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

address::address(u32 addr, u16 port) : m_ip(htonl(addr)), m_port(htonl(port)) {}
address::address(const char *addr, u16 port) :
  m_ip(inet_addr(addr)), m_port(htons(port)) {}

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
  out.sin_addr.s_addr = addr.m_ip;
  out.sin_port = addr.m_port;
  return out;
}

int socket::receive(address &addr, buffer &buf) const {
  const auto outaddr = makeaddress(addr);
  socklen_t addrlen = sizeof(outaddr);
  const int flags = MSG_NOSIGNAL;
  buf.len = recvfrom(fd, buf.data, BUF_SZ, flags, (sockaddr*)&outaddr, &addrlen);
  if (buf.len != u32(-1)) {
    addr.m_port = outaddr.sin_port;
    addr.m_ip = outaddr.sin_addr.s_addr;
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

static u32 seed = 0;
void start() { seed = time(NULL); }
void end() {}
static u32 gettime() {
  struct timeval timeval;
  gettimeofday (&timeval, NULL);
  return timeval.tv_sec * 1000 + timeval.tv_usec / 1000;
}

/// NOTE
// mettre rel/unrel ensemble mais ne renvoyer que la partie rel s'il y a eu une
// perte de ce paquet. ajouter un u16 pour l'offset de unrel dans le paquet

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
  u32 m_seq_reliable;   // 31 bits for sequence. 1 bit to know if reliable or not
  u32 m_ackseq_receipt; // 31 bits to acknowledge sequence. 1 bit for reliable ack
  u16 m_qport;          // handles port renaming by routers
  char m_data[];        // message itself
};

struct internal_channel {
  internal_channel(u32 ip, u16 port, struct internal_server *owner = NULL);
  ~internal_channel();
  bool istimedout(u32 curtime, u32 maxtimeout);
  void send(bool reliable, const char *fmt, va_list args);
  int rcv(const char *fmt, va_list args);
  ringbuffer<internal_msg*, MAX_RELIABLE_NUM> m_outstanding_rel;
  struct internal_server *m_owner; // set only for server->client communication
  struct internal_msg *m_incoming; // message to process
  vector<char> m_msg[2];   // current reliable and unreliable messages
  vector<u32> m_offset[2]; // position of each individual message in m_msg
  u32 m_ip;          // ip of the remote machine
  u16 m_qport;       // qport of the remote to fight port renaming by routers
  u32 m_seq:31;      // next sequence number to remote
  u32 m_ack_rel:1;   // bit to send that the remote has to send back for rel
  u32 m_last_seq:31; // the larger sequence number we got from this remote machine
  u32 m_last_ack:1;  // last reliable ack we got from the remote machine
  u32 m_last_update; // time of last message from remote
};

struct internal_server {
  internal_server(u32 maxclients, u32 timedout, u16 port);
  void remove(internal_channel*);
  internal_channel *active();
  internal_channel *timedout();
  internal_channel *findchannel(u32 ip, u16 qport) const;
  vector<internal_channel*> m_channels;
  socket m_sock;
  u32 m_maxclients;
  u32 m_maxtimeout;
};

internal_msg *internal_msg::create(const buffer &buf) {
  auto msg = (internal_msg *) malloc(buf.len);
  memcpy(msg, buf.data, buf.len);
  return msg;
}
void internal_msg::destroy(internal_msg *msg) { free(msg); }

internal_channel::internal_channel(u32 ip, u16 qport, internal_server *owner) :
  m_owner(owner), m_incoming(NULL), m_ip(ip), m_qport(qport),
  m_seq(0), m_ack_rel(0), m_last_seq(0), m_last_ack(0),
  m_last_update(gettime()) {}

internal_channel::~internal_channel() {
  if (m_incoming != NULL) internal_msg::destroy(m_incoming);
  if (m_owner != NULL) m_owner->remove(this);
}

bool internal_channel::istimedout(u32 curtime, u32 maxinactivity) {
  return m_outstanding_rel.n == MAX_RELIABLE_NUM ||
    curtime - m_last_update > maxinactivity;
}

void internal_channel::send(bool reliable, const char *fmt, va_list args) {
  auto &msg = m_msg[reliable?1:0];
  m_offset[reliable?1:0].add(msg.size());
  while (char ch = *fmt) {
    switch (ch) {
#define SEND(CHAR, TYPE, VA_ARG_TYPE) case CHAR: {\
  union { TYPE x; char bytes[sizeof(TYPE)]; };\
  x = TYPE(va_arg(args, VA_ARG_TYPE));\
  loopi(sizeof(TYPE)) msg.add(bytes[i]);\
  break;\
}
      SEND('c', char, int)
      SEND('s', short, int)
      SEND('i', int, int)
#undef SEND
      default: sys::fatal("unknown format string");
    }
    ++fmt;
  }
}

int internal_channel::rcv(const char *fmt, va_list args) {
#if 0
  if (m_incoming == NULL) return 0;
  u32 matched = 0;
  while (char ch = *fmt) {
    switch (ch) {
#define SEND(CHAR, TYPE, VA_ARG_TYPE) case CHAR: {\
  union { TYPE x; char bytes[sizeof(TYPE)]; };\
  x = TYPE(va_arg(args, VA_ARG_TYPE));\
  loopi(sizeof(TYPE)) msg.add(bytes[i]);\
  break;\
}
      SEND('c', char, int)
      SEND('s', short, int)
      SEND('i', int, int)
#undef SEND
      default: sys::fatal("unknown format string");
    }
    ++fmt;
  }
#endif
  return 0;
// METS L'UPDATE ICI (quand tout a ete consomme)! Comme ca, on  checke aussi
// l'integrite et on peut timeout sur des paquets corrompus
}

internal_server::internal_server(u32 maxclients, u32 maxtimeout, u16 port) :
  m_sock(port), m_maxclients(maxclients), m_maxtimeout(maxtimeout) {}

internal_channel *internal_server::findchannel(u32 ip, u16 qport) const {
  for (auto c : m_channels) if (c->m_ip==ip && c->m_qport==qport) return c;
  return NULL;
}

internal_channel *internal_server::active() {
  buffer buf;
  address addr;
  const u32 curtime = gettime();
  for (;;) {
    const int len = m_sock.receive(addr, buf);
    if (len == -1) break;
    const auto msg = (internal_msg*) buf.data;

    // allocate a new channel if not already handled
    internal_channel *c;
    if ((c = findchannel(addr.m_ip, msg->m_qport)) == NULL) {
      if (u32(m_channels.size()) == m_maxclients) continue;
      c = m_channels.add(new internal_channel(addr.m_ip, msg->m_qport));
    }
    if (c->istimedout(curtime, m_maxtimeout)) continue;
    c->m_last_seq = ntohl(msg->m_seq_reliable) & 0x7fffffff;
    c->m_last_ack = ntohl(msg->m_seq_reliable) >> 31;
    c->m_incoming = internal_msg::create(buf);
    c->m_last_update = curtime;
    return c;
  }
  return NULL;
}

internal_channel *internal_server::timedout() {
  const u32 curtime = gettime();
  for (auto c : m_channels) if (c->istimedout(curtime, m_maxtimeout)) return c;
  return NULL;
}

void internal_server::remove(internal_channel *chan) {
  u32 idx = 0, n = u32(m_channels.size());
  for (; idx < n; ++idx) if (chan == m_channels[idx]) break;
  if (idx == n) return;
  m_channels[idx] = m_channels[n-1];
  m_channels.setsize(n-1);
}

} /* namespace net */
} /* namespace q */

