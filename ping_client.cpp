#include "net.hpp"
#include "net_protocol.hpp"
#include <unistd.h>

#if defined(__WIN32__)
float millis() {
  LARGE_INTEGER freq, val;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&val);ss
  static double first = double(val.QuadPart) / double(freq.QuadPart) * 1e3;
  return float(double(val.QuadPart) / double(freq.QuadPart) * 1e3 - first);
}
#else
float millis() {
  struct timeval tp; gettimeofday(&tp,NULL);
  static double first = double(tp.tv_sec)*1e3 + double(tp.tv_usec)*1e-3;
  return float(double(tp.tv_sec)*1e3 + double(tp.tv_usec)*1e-3 - first);
}
#endif

int main(int argc, const char *argv[]) {
  using namespace q;
  net::start();
  // const net::address addr("2.25.198.185", 50000);
  const net::address addr("127.0.0.1", 1234);
  const auto chan = net::channel::create(addr, 50000);
  loopi(100) {
    float start = millis();
    chan->send(false, "i", 1);
    chan->flush();
    int x;
    while (chan->rcv("i", &x) == 0);
    printf("one %i %f\n", x, millis() - start);
  }
  net::channel::destroy(chan);
  net::end();
  return 0;
}


