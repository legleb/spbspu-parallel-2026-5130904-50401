#include "command.hpp"

#include <cstddef>
#include <map>
#include <ostream>
#include <string>
#include <vector>
#include <utility>
#include <cctype>
#include <chrono>
#include <cerrno>
#include <csignal>

#include <iomanip>
#include <poll.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include "parse.hpp"
#include "process.hpp"
#include "shape.hpp"

namespace {
  void printShape(const sedov::Shape& s, std::ostream& out)
  {
    if (s.kind == sedov::Kind::Circle) {
      out << s.a << " (" << s.x << " " << s.y << ")\n";
    } else {
      out << s.a << " " << s.b << " (" << s.x << " " << s.y << ") " << std::fixed << std::setprecision(3) << s.angle
          << "\n";
    }
  }
}

sedov::CommandTable sedov::makeCommandTable()
{
  CommandTable table;
  table.emplace("circle", cmdCircle);
  table.emplace("show", cmdShow);
  table.emplace("set", cmdSet);
  table.emplace("showset", cmdShowset);
  table.emplace("frame", cmdFrame);
  table.emplace("frameset", cmdFrameset);
  table.emplace("spawn", cmdSpawn);
  table.emplace("area-on", cmdAreaOn);
  table.emplace("wait", cmdWait);
  table.emplace("status", cmdStatus);
  table.emplace("ellipse", cmdEllipse);
  table.emplace("rotate", cmdRotate);
  table.emplace("timeout", cmdTimeout);
  table.emplace("kill", cmdKill);
  return table;
}

void sedov::printCalcResult(const Calc& calc, std::ostream& out)
{
  if (calc.status == Status::InProgress) {
    out << calc.process_name << " <IN PROGRESS>\n";
  } else if (calc.status == Status::Canceled) {
    out << calc.process_name << " <CANCELED>\n";
  } else {
    out << calc.process_name << " " << calc.time_ms << " " << std::fixed << std::setprecision(3) << calc.area << "\n";
  }
}

bool sedov::tryReadResponse(CommandContext& ctx, int fd)
{
  Response resp{};
  if (!recvFull(fd, &resp, sizeof(resp))) {
    return false;
  }

  const std::map< size_t, std::string >::iterator it = ctx.calc_id_to_name.find(resp.calc_id);
  if (it == ctx.calc_id_to_name.end()) {
    return false;
  }

  const CalcMap::iterator calc_it = ctx.calcs.find(it->second);
  if (calc_it != ctx.calcs.end()) {
    calc_it->second.status = Status::Done;
    calc_it->second.time_ms = resp.time_ms;
    calc_it->second.area = resp.area;
  }
  return true;
}

void sedov::cmdCircle(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out)
{
  const size_t countOfWords = 5;
  if (words.size() != countOfWords) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (words[1].empty() || !std::isalpha(static_cast< char >(words[1][0]))) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (ctx.shapes.count(words[1]) > 0) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  long long r = 0, x = 0, y = 0;
  if (!parseLongLong(words[2], r) || !parseLongLong(words[3], x) || !parseLongLong(words[4], y)) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (r <= 0) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  ctx.shapes.emplace(words[1], Shape{Kind::Circle, r, r, x, y, 0.0});
}

void sedov::cmdEllipse(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out)
{
  const size_t countOfWords = 6;
  if (words.size() != countOfWords) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (words[1].empty() || !std::isalpha(static_cast< unsigned char >(words[1][0]))) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (ctx.shapes.count(words[1]) > 0) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  long long a = 0, b = 0, x = 0, y = 0;
  if (!parseLongLong(words[2], a) || !parseLongLong(words[3], b) || !parseLongLong(words[4], x)
      || !parseLongLong(words[countOfWords - 1], y)) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (a <= 0 || b <= 0) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  ctx.shapes.emplace(words[1], Shape{Kind::Ellipse, a, b, x, y, 0.0});
}

void sedov::cmdRotate(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out)
{
  if (words.size() != 3) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  const ShapeMap::iterator it = ctx.shapes.find(words[1]);
  if (it == ctx.shapes.end()) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  double ksi = 0.0;
  if (!parseDouble(words[2], ksi)) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  it->second.angle += ksi;
}

void sedov::cmdShow(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out)
{
  if (words.size() != 2) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  const ShapeMap::iterator it = ctx.shapes.find(words[1]);
  if (it == ctx.shapes.end()) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  printShape(it->second, out);
}

void sedov::cmdSet(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out)
{
  if (words.size() < 3) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (words[1].empty() || !std::isalpha(static_cast< char >(words[1][0]))) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  long long n = 0;
  if (!parseLongLong(words[2], n) || n <= 0) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (words.size() != 3 + static_cast< size_t >(n)) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  for (size_t i = 0; i < static_cast< size_t >(n); ++i) {
    if (ctx.shapes.count(words[3 + i]) == 0) {
      out << "<INVALID COMMAND>\n";
      return;
    }
  }
  if (ctx.sets.count(words[1]) > 0) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  std::vector< std::string > v(words.begin() + 3, words.begin() + 3 + n);
  ctx.sets.emplace(words[1], std::move(v));
}

void sedov::cmdShowset(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out)
{
  if (words.size() != 2) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  const SetMap::iterator it = ctx.sets.find(words[1]);
  if (it == ctx.sets.end()) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  for (std::vector< std::string >::const_iterator name_it = it->second.begin(); name_it != it->second.end();
      ++name_it) {
    const Shape& s = ctx.shapes.at(*name_it);
    printShape(s, out);
  }
}

void sedov::cmdFrame(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out)
{
  if (words.size() != 2) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  const ShapeMap::iterator it = ctx.shapes.find(words[1]);
  if (it == ctx.shapes.end()) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  const Box box = boundingBox(it->second);
  out << "(" << box.x_min << " " << box.y_min << ") (" << box.x_max << " " << box.y_max << ")\n";
}

void sedov::cmdFrameset(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out)
{
  if (words.size() != 2) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  const SetMap::iterator it = ctx.sets.find(words[1]);
  if (it == ctx.sets.end()) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  std::vector< Shape > shapes;
  shapes.reserve(it->second.size());
  for (std::vector< std::string >::const_iterator name_it = it->second.begin(); name_it != it->second.end();
      ++name_it) {
    shapes.push_back(ctx.shapes.at(*name_it));
  }
  const Box box = boundingBox(shapes);
  out << "(" << box.x_min << " " << box.y_min << ") (" << box.x_max << " " << box.y_max << ")\n";
}

void sedov::cmdSpawn(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out)
{
  if (words.size() != 3) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (words[1].empty() || !std::isalpha(static_cast< char >(words[1][0]))) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (ctx.processes.count(words[1]) > 0) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  long long seed = 0;
  if (!parseLongLong(words[2], seed) || seed < 0) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  int in_pipe[2] = {-1, -1};
  int out_pipe[2] = {-1, -1};
  if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) {
    out << "<INVALID COMMAND>\n";
    if (in_pipe[0] != -1) {
      close(in_pipe[0]);
      close(in_pipe[1]);
    }
    if (out_pipe[0] != -1) {
      close(out_pipe[0]);
      close(out_pipe[1]);
    }
    return;
  }
  const pid_t pid = fork();
  if (pid < 0) {
    out << "<INVALID COMMAND>\n";
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(out_pipe[0]);
    close(out_pipe[1]);
    return;
  }
  if (!pid) {
    close(in_pipe[1]);
    close(out_pipe[0]);
    childLoop(in_pipe[0], out_pipe[1], static_cast< size_t >(seed));
    _exit(0);
  }
  close(in_pipe[0]);
  close(out_pipe[1]);
  ctx.processes.emplace(words[1], Process{pid, in_pipe[1], out_pipe[0], true});
}

void sedov::cmdAreaOn(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out)
{
  const size_t countOfWords = 6;
  if (words.size() != countOfWords) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (words[1].empty() || !std::isalpha(static_cast< char >(words[1][0]))) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  const std::string& proc_name = words[1];
  const std::string& calc_name = words[2];
  const std::string& set_name = words[3];
  const ProcessMap::iterator proc_it = ctx.processes.find(proc_name);
  if (proc_it == ctx.processes.end() || !proc_it->second.alive) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (ctx.calcs.count(calc_name) > 0) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  const SetMap::iterator set_it = ctx.sets.find(set_name);
  if (set_it == ctx.sets.end()) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  long long threads_ll = 0, tries_ll = 0;
  if (!parseLongLong(words[4], threads_ll) || threads_ll <= 0) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (!parseLongLong(words[countOfWords - 1], tries_ll) || tries_ll <= 0) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  std::vector< Shape > shapes;
  shapes.reserve(set_it->second.size());
  for (std::vector< std::string >::const_iterator shape_it = set_it->second.begin(); shape_it != set_it->second.end();
      ++shape_it) {
    const ShapeMap::iterator it = ctx.shapes.find(*shape_it);
    if (it == ctx.shapes.end()) {
      out << "<INVALID COMMAND>\n";
      return;
    }
    shapes.push_back(it->second);
  }
  RequestHeader header{};
  header.type = RequestType::Area;
  header.calc_id = ctx.next_calc_id++;
  header.tries = static_cast< size_t >(tries_ll);
  header.threads = static_cast< size_t >(threads_ll);
  header.n_shapes = shapes.size();
  if (!sendFull(proc_it->second.in_fd, &header, sizeof(header))) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (!shapes.empty()) {
    if (!sendFull(proc_it->second.in_fd, shapes.data(), sizeof(Shape) * shapes.size())) {
      out << "<INVALID COMMAND>\n";
      return;
    }
  }
  Calc calc{};
  calc.process_name = proc_name;
  calc.status = Status::InProgress;
  calc.time_ms = 0;
  calc.area = 0.0;
  ctx.calcs.emplace(calc_name, calc);
  ctx.calc_id_to_name.emplace(header.calc_id, calc_name);
}

void sedov::cmdWait(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out)
{
  if (words.size() != 2) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  const std::string& calc_name = words[1];
  const CalcMap::iterator calc_it = ctx.calcs.find(calc_name);
  if (calc_it == ctx.calcs.end()) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (calc_it->second.status != Status::InProgress) {
    printCalcResult(calc_it->second, out);
    return;
  }
  const std::string& proc_name = calc_it->second.process_name;
  const ProcessMap::iterator proc_it = ctx.processes.find(proc_name);
  if (proc_it == ctx.processes.end() || !proc_it->second.alive) {
    calc_it->second.status = Status::Canceled;
    printCalcResult(calc_it->second, out);
    return;
  }
  const int fd = proc_it->second.out_fd;
  while (true) {
    struct pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;
    const int ret = poll(&pfd, 1, -1);
    if (ret < 0) {
      continue;
    }
    if (pfd.revents & (POLLIN | POLLHUP)) {
      if (!tryReadResponse(ctx, fd)) {
        calc_it->second.status = Status::Canceled;
        break;
      }
    }
    if (calc_it->second.status != Status::InProgress) {
      break;
    }
  }
  printCalcResult(calc_it->second, out);
}

void sedov::cmdStatus(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out)
{
  if (words.size() != 2) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  const std::string& calc_name = words[1];
  const CalcMap::iterator calc_it = ctx.calcs.find(calc_name);
  if (calc_it == ctx.calcs.end()) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (calc_it->second.status == Status::InProgress) {
    const std::string& proc_name = calc_it->second.process_name;
    const ProcessMap::iterator proc_it = ctx.processes.find(proc_name);
    if (proc_it != ctx.processes.end() && proc_it->second.alive) {
      const int fd = proc_it->second.out_fd;
      while (true) {
        struct pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;
        const int ret = poll(&pfd, 1, 0);
        if (ret <= 0) {
          break;
        }
        if (pfd.revents & (POLLIN | POLLHUP)) {
          if (!tryReadResponse(ctx, fd)) {
            calc_it->second.status = Status::Canceled;
            break;
          }
        }
      }
    } else {
      calc_it->second.status = Status::Canceled;
    }
  }
  printCalcResult(calc_it->second, out);
}

void sedov::cmdTimeout(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out)
{
  if (words.size() != 3) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  const std::string& calc_name = words[1];
  long long ms = 0;
  if (!parseLongLong(words[2], ms) || ms < 0) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  const CalcMap::iterator calc_it = ctx.calcs.find(calc_name);
  if (calc_it == ctx.calcs.end()) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  if (calc_it->second.status != Status::InProgress) {
    printCalcResult(calc_it->second, out);
    return;
  }
  const std::string& proc_name = calc_it->second.process_name;
  const ProcessMap::iterator proc_it = ctx.processes.find(proc_name);
  if (proc_it == ctx.processes.end() || !proc_it->second.alive) {
    calc_it->second.status = Status::Canceled;
    printCalcResult(calc_it->second, out);
    return;
  }
  const int fd = proc_it->second.out_fd;
  const std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
  const std::chrono::milliseconds total_timeout(ms);
  while (calc_it->second.status == Status::InProgress) {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    const std::chrono::milliseconds elapsed = std::chrono::duration_cast< std::chrono::milliseconds >(now - start_time);
    if (elapsed >= total_timeout) {
      break;
    }
    const auto remaining = std::chrono::duration_cast< std::chrono::milliseconds >(total_timeout - elapsed).count();
    struct pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;
    const int ret = poll(&pfd, 1, static_cast< int >(remaining));
    if (ret < 0) {
      if (errno == EINTR) {
        continue;
      }
      calc_it->second.status = Status::Canceled;
      break;
    }
    if (ret == 0) {
      break;
    }
    if (pfd.revents & (POLLIN | POLLHUP)) {
      if (!tryReadResponse(ctx, fd)) {
        calc_it->second.status = Status::Canceled;
        break;
      }
    }
  }
  printCalcResult(calc_it->second, out);
}

void sedov::cmdKill(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out)
{
  if (words.size() != 2) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  const std::string& proc_name = words[1];
  const ProcessMap::iterator proc_it = ctx.processes.find(proc_name);
  if (proc_it == ctx.processes.end() || !proc_it->second.alive) {
    out << "<INVALID COMMAND>\n";
    return;
  }
  for (CalcMap::iterator it = ctx.calcs.begin(); it != ctx.calcs.end(); ++it) {
    if (it->second.process_name == proc_name && it->second.status == Status::InProgress) {
      it->second.status = Status::Canceled;
    }
  }
  kill(proc_it->second.pid, SIGTERM);
  close(proc_it->second.in_fd);
  close(proc_it->second.out_fd);
  waitpid(proc_it->second.pid, nullptr, 0);
  proc_it->second.alive = false;
}
