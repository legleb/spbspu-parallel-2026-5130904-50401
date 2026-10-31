#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <map>
#include <ostream>
#include <string>
#include <vector>
#include <cstddef>

#include "process.hpp"
#include "shape.hpp"

namespace sedov {
  enum class Status { InProgress, Done, Canceled };

  struct Calc {
    std::string process_name;
    Status status;
    size_t time_ms;
    double area;
  };

  using CalcMap = std::map< std::string, Calc >;

  struct CommandContext {
    ShapeMap shapes;
    SetMap sets;
    ProcessMap processes;
    CalcMap calcs;
    std::map< size_t, std::string > calc_id_to_name;
    size_t next_calc_id = 1;
  };

  using CommandHandler = void (*)(const std::vector< std::string >&, CommandContext&, std::ostream&);

  using CommandTable = std::map< std::string, CommandHandler >;

  CommandTable makeCommandTable();

  void cmdCircle(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out);
  void cmdShow(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out);
  void cmdSet(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out);
  void cmdShowset(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out);
  void cmdFrame(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out);
  void cmdFrameset(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out);
  void cmdSpawn(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out);
  void cmdAreaOn(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out);
  void cmdWait(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out);
  void cmdStatus(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out);
  void cmdEllipse(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out);
  void cmdRotate(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out);
  void cmdTimeout(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out);
  void cmdKill(const std::vector< std::string >& words, CommandContext& ctx, std::ostream& out);

  bool tryReadResponse(CommandContext& ctx, int fd);
  void printCalcResult(const Calc& calc, std::ostream& out);
}

#endif
