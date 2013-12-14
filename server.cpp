#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
#if 0
namespace q {
namespace net {

static const u32 MTU = 1400;
static const u32 BUF_SZ = MTU - 128;

// ipv4 address
typedef sockaddr_in address;

// buffers are fixed-size-allocated for maximum simplicity
struct buffer {
  buffer(int len = 0, char *ptr = NULL) : len(len) {
    if (ptr && len != 0) memcpy(data, ptr, len);
  }
  int len;
  char data[BUF_SZ];
};

// very basic udp socket interface
struct socket {
  socket() : fd(::socket(PF_INET, SOCK_DGRAM, 0)) {
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
  void bind(u16 port) {
    struct sockaddr_in servaddr;
    memset(&servaddr,0,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    servaddr.sin_port=htons(port);
    ::bind(fd, (sockaddr*) &servaddr, sizeof(servaddr));
  }
  int receive(address &addr, buffer &buf) {
    socklen_t addrlen = sizeof(addr);
    const int flags = MSG_NOSIGNAL;
    const auto len = recvfrom(fd, buf.data, BUF_SZ, flags, (sockaddr*)&addr, &addrlen);
    if (len != -1) return len;
    if (errno != EWOULDBLOCK) sys::fatal("net: error receiving data on socket");
    return buf.len = 0;
  }
  int send(const address &addr, const buffer &buf) {
    const socklen_t addrlen = sizeof(addr);
    const auto len = sendto(fd, buf.data, buf.len, MSG_NOSIGNAL, (const sockaddr*) &addr, addrlen);
    if (len != -1) return len;
    if (errno != EWOULDBLOCK) sys::fatal("net: error sending data on socket");
    return 0;
  }
  int fd;
};

socket create_server(u16 port) {
  socket sock;
  sock.bind(port);
  return sock;
}
socket create_client() { return socket(); }

} /* namespace net */
} /* namespace q */
#endif

int main(int argc, char**argv)
{
  using namespace q;
#if 0
  int sockfd,n;
  struct sockaddr_in servaddr,cliaddr;
  socklen_t len;
  char mesg[1000];

  sockfd=socket(AF_INET,SOCK_DGRAM,0);

  bzero(&servaddr,sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
  servaddr.sin_port=htons(32000);
  bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr));

  for (;;)
  {
    struct msghdr hdr;
    bzero(&hdr, sizeof(hdr));
    len = sizeof(cliaddr);
    n = recvfrom(sockfd,mesg,1000,0,(struct sockaddr *)&cliaddr,&len);
    // n = recvmsg(sockfd,&hdr,mesg,1000,0,(struct sockaddr *)&cliaddr,&len);
    sendto(sockfd,mesg,n,0,(struct sockaddr *)&cliaddr,sizeof(cliaddr));
    printf("-------------------------------------------------------\n");
    mesg[n] = 0;
    printf("Received the following:\n");
    printf("%s",mesg);
    printf("-------------------------------------------------------\n");
  }
#else
  auto server = net::create_server(1234);
  for (;;) {
    net::buffer buf;
    net::address addr;
    if (server.receive(addr, buf) == 0) continue;
    printf("received: %s", buf.data);
    buf.len = sprintf(buf.data, "This is OK!\n");
    server.send(addr, buf);
  }
  return 0;
#endif
}

