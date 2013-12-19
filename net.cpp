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
  buf.len = recvfrom(fd, buf.data, MTU, flags, (sockaddr*)&outaddr, &addrlen);
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
  buf.len = recvfrom(fd, buf.data, MTU, flags, NULL, NULL);
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

/*-------------------------------------------------------------------------
 - implements network protocol
 -------------------------------------------------------------------------*/
// we do not store more than 128 reliable *outstanding* reliable messages:
// beyond that, we just consider that the channel timed out. the user will be
// responsible to destroy it
static const u32 MAX_RELIABLE_NUM = 128;

struct internal_msg;
struct internal_server;
struct internal_channel;

struct internal_msg {
  static internal_msg *create(const buffer&);
  static void destroy(internal_msg*);
  INLINE u32 rel() const { return ntohl(m_seq_rel) >> 31; }
  INLINE u32 seq() const { return ntohl(m_seq_rel) & 0x7fffffff; }
  INLINE u32 ackrel() const { return ntohl(m_ackseq_ackrel) >> 31; }
  INLINE u32 ackseq() const { return ntohl(m_ackseq_ackrel) & 0x7fffffff; }
  u32 m_seq_rel;       // 31 bits for sequence. 1 bit to know if reliable or not
  u32 m_ackseq_ackrel; // 31 bits to acknowledge sequence. 1 bit for reliable ack
  u16 m_qport;         // handles port renaming by routers
  char m_data[];       // message itself
};
static const u32 MAX_MSG_SZ = MTU - sizeof(internal_msg);

struct internal_channel {
  internal_channel(u32 ip, u16 port, u16 qport, internal_server *owner = NULL);
  internal_channel(u32 ip, u16 port, u16 qport, u32 maxtimeout);
  ~internal_channel();
  void init(u32 ip, u16 port, u16 qport);
  bool istimedout(u32 curtime = gettime());
  void send(bool reliable, const char *fmt, va_list args);
  int rcv(const char *fmt, va_list args);
  void flush();
  internal_server *m_owner; // set only for server->client communication
  socket*m_sock;            // owned by the server for a server channel

  /* incoming message */
  internal_msg *m_incoming; // message to process
  u32 m_incoming_offset;    // offset where to pull data from msg
  u32 m_incoming_len;       // size of the incoming message

  /* outgoing messages (reliable and not reliable */
  enum { REL, UNREL, MSG_NUM };
  vector<char> m_msg[MSG_NUM];   // current reliable and unreliable messages
  vector<u32> m_offset[MSG_NUM]; // positions of each individual message in m_msg

  /* we save reliable data until we know it was received */
  ringbuffer<pair<char*,u32>, MAX_RELIABLE_NUM> m_outstanding_rel;

  /* connection information */
  u32 m_maxtimeout;  // millis after which we consider channel is dead
  u32 m_ip;          // ip of the remote machine
  u16 m_port;        // actual port of the remote machine
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

internal_channel::internal_channel(u32 ip, u16 port, u16 qport, internal_server *owner) :
  m_owner(owner), m_sock(&owner->m_sock), m_maxtimeout(owner->m_maxtimeout)
{init(ip, port, qport);}

internal_channel::internal_channel(u32 ip, u16 port, u16 qport, u32 maxtimeout) :
  m_owner(NULL), m_sock(new socket), m_maxtimeout(maxtimeout)
{init(ip, port, qport);}

void internal_channel::init(u32 ip, u16 port, u16 qport) {
  m_incoming = NULL;
  m_incoming_offset = 0;
  m_incoming_len = 0;
  m_ip = ip;
  m_port = port;
  m_qport = qport;
  m_seq = 0;
  m_ack_rel = 0;
  m_last_seq = 0;
  m_last_ack = 0;
  m_last_update = gettime();
}

internal_channel::~internal_channel() {
  if (m_incoming != NULL) internal_msg::destroy(m_incoming);
  if (m_owner != NULL)
    m_owner->remove(this);
  else
    delete m_sock;
}

bool internal_channel::istimedout(u32 curtime) {
  return m_outstanding_rel.full() || curtime-m_last_update > m_maxtimeout;
}

void internal_channel::send(bool reliable, const char *fmt, va_list args) {
  auto &msg = m_msg[reliable?REL:UNREL];
  const auto offset = msg.size();
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
      default: sys::fatal("net: unknown format string");
    }
    ++fmt;
  }
  if (msg.size()>offset)
    m_offset[reliable?REL:UNREL].add(offset);
}

int internal_channel::rcv(const char *fmt, va_list args) {
  if (m_incoming == NULL) return 0;
  const u32 initial_offset = m_incoming_offset;
  while (char ch = *fmt) {
    switch (ch) {
#define RCV(CHAR, TYPE)\
      case CHAR: {\
        const auto dst = va_arg(args, char*);\
        if (m_incoming_offset+sizeof(TYPE) > m_incoming_len) goto error;\
        loopi(sizeof(TYPE))\
          dst[i] = m_incoming->m_data[m_incoming_offset++];\
      }
      RCV('c', char)
      RCV('s', short)
      RCV('i', int)
#undef RCV
      default: sys::fatal("net: unknown format string");
    }
    ++fmt;
  }
  // we processed the message completely; as far as we know, there is no
  // corruption, so we can update the ack data accordingly
  if (m_incoming_offset == m_incoming_len) {
    if (m_incoming->rel()) m_last_ack ^= 1;
    m_last_seq = max(m_last_seq, ntohl(m_incoming->seq()));
    m_last_update = gettime();
  }
  return m_incoming_offset - initial_offset;
error:
  return 0;
}

static void chan_make_header(internal_channel *c, buffer &buf, bool rel) {
  buf.append(htonl(c->m_seq++ | ((rel?1:0)<<31)));
  buf.append(htonl(c->m_last_seq | (c->m_last_ack<<31)));
  buf.append(c->m_qport);
}

void internal_channel::flush() {
  // skip it completely if channel timed out
  if (istimedout()) {
    loopi(MSG_NUM) {
      m_msg[i].setsize(0);
      m_offset[i].setsize(0);
    }
    return;
  }

  // nothing to do. we are good
  if (m_outstanding_rel.empty() && m_msg[0].size()==0 && m_msg[1].size()==0)
    return;

  // we first push the reliable data if any
  m_outstanding_rel.addback() = m_msg[REL].move();
  const auto rel = m_outstanding_rel.front();
  buffer buf;
  if (rel.second > MAX_MSG_SZ)
    sys::fatal("net: reliable message is too large");
  else if (rel.second > 0) {
    chan_make_header(this, buf, true);
    buf.copy(rel.first, rel.second);
  } else
    chan_make_header(this, buf, false);

  // now, we append the unreliable data, using as many buffers as we need
  const auto unrel = m_msg[UNREL].move();
  const auto &offsets = m_offset[UNREL];
  loopv(offsets) {
    u32 sz;
    if (i==offsets.size()-1)
      sz = unrel.second - offsets[i];
    else
      sz = offsets[i+1] - offsets[i];
    if (buf.len+sz > MTU) {
      m_sock->send(address(m_ip, m_port), buf);
      buf.len = 0;
      if (i<offsets.size()-1) chan_make_header(this, buf, false);
    }
  }
  if (buf.len > 0) m_sock->send(address(m_ip, m_port), buf);
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
  for (;;) {
    const int len = m_sock.receive(addr, buf);
    if (len == -1) break;
    const auto msg = (internal_msg*) buf.data;

    // allocate a new channel if not already handled
    internal_channel *c;
    if ((c = findchannel(addr.m_ip, msg->m_qport)) == NULL) {
      if (u32(m_channels.size()) == m_maxclients) continue;
      c = m_channels.add(new internal_channel(addr.m_ip, addr.m_port, msg->m_qport));
    }

    // skip the channel if dead or if we already got a more recent package
    if (c->istimedout() || c->m_incoming->seq()<=c->m_last_seq)
      continue;
    c->m_incoming = internal_msg::create(buf);
    return c;
  }
  return NULL;
}

internal_channel *internal_server::timedout() {
  const u32 curtime = gettime();
  for (auto c : m_channels) if (c->istimedout(curtime)) return c;
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

