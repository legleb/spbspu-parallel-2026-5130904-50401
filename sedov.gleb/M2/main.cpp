#include <csignal>
#include <iostream>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include "command.hpp"
#include "parse.hpp"
#include "process.hpp"

int main()
{
  struct sigaction sa{};
  sa.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sa, nullptr);
  sedov::CommandContext ctx;
  const sedov::CommandTable table = sedov::makeCommandTable();
  std::string line;
  while (std::getline(std::cin, line)) {
    const std::vector< std::string > words = sedov::split(line);
    if (words.empty()) {
      continue;
    }
    const sedov::CommandTable::const_iterator it = table.find(words[0]);
    if (it == table.end()) {
      std::cout << "<UNKNOWN COMMAND>\n";
      continue;
    }
    it->second(words, ctx, std::cout);
  }
  for (sedov::ProcessMap::iterator it = ctx.processes.begin(); it != ctx.processes.end(); ++it) {
    const sedov::Process& proc = it->second;
    if (proc.alive) {
      kill(proc.pid, SIGTERM);
      close(proc.in_fd);
      close(proc.out_fd);
      waitpid(proc.pid, nullptr, 0);
    }
  }
  return 0;
}
