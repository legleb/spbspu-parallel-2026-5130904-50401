#ifndef PROCESS_HPP
#define PROCESS_HPP

#include <cstddef>
#include <map>
#include <string>

#include <sys/types.h>

namespace sedov {
  struct Process {
    pid_t pid;
    int in_fd;
    int out_fd;
    bool alive;
  };

  using ProcessMap = std::map< std::string, Process >;

  void childLoop(int in_fd, int out_fd, size_t seed);

  enum class RequestType : int { Area = 1, Cancel = 2, Exit = 3 };

  struct RequestHeader {
    RequestType type;
    size_t calc_id;
    size_t tries;
    size_t threads;
    size_t n_shapes;
  };

  struct Response {
    size_t calc_id;
    size_t time_ms;
    double area;
  };

  bool sendFull(int fd, const void* buf, size_t k);
  bool recvFull(int fd, void* buf, size_t k);
}

#endif
