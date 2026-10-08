#include <iostream>
#include <random>
#include <future>
#include <vector>
#include <algorithm>
#include <functional>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <new>

namespace sedov {
  struct Circle {
    long long r, x, y;
  };

  std::vector< Circle > readCircles(std::istream& in);

  struct Box {
    long long x_min, x_max, y_min, y_max;
  };

  Box calcBox(const std::vector< Circle >& circles);

  struct Result {
    size_t hits_union, hits_inter;
  };

  bool isInside(double dx, double dy, double r);
  Result calc(const std::vector< Circle >& circles, const Box& box, size_t tests, size_t seed);
  Result countTotalHits(
      const std::vector< Circle >& circles, const Box& box, size_t threads, size_t tries, size_t seed);
}

int main(int argc, char** argv)
{
  long long threads = 0, tries = 0, seed = 0;
  if (argc < 3 || argc > 4) {
    std::cerr << "Incorrect number of arguments\n";
    return 1;
  }
  try {
    threads = std::stoll(argv[1]);
    tries = std::stoll(argv[2]);
    if (argc == 4) {
      seed = std::stoll(argv[3]);
    }
  } catch (const std::invalid_argument& ia) {
    std::cerr << ia.what() << '\n';
    return 1;
  } catch (const std::out_of_range& oor) {
    std::cerr << oor.what() << '\n';
    return 1;
  }
  if (threads < 0 || tries <= 0 || seed < 0) {
    std::cerr << "Threads, tries and seed must be positive\n";
    return 1;
  }
  threads = threads > 0 ? threads : 1;

  std::vector< sedov::Circle > circles;
  try {
    circles = sedov::readCircles(std::cin);
  } catch (const std::invalid_argument& ia) {
    std::cerr << ia.what() << '\n';
    return 2;
  } catch (const std::bad_alloc& ba) {
    std::cerr << ba.what() << '\n';
    return 2;
  }
  if (circles.empty()) {
    std::cout << 0 << ' ' << 0 << '\n';
    return 0;
  }

  const sedov::Box box = sedov::calcBox(circles);

  sedov::Result hits{0, 0};
  try {
    hits = sedov::countTotalHits(circles, box, threads, tries, seed);
  } catch (const std::system_error& se) {
    std::cerr << se.what() << '\n';
    return 2;
  } catch (const std::bad_alloc& ba) {
    std::cerr << ba.what() << '\n';
    return 2;
  }

  const double square = (static_cast< double >(box.x_max) - static_cast< double >(box.x_min))
      * (static_cast< double >(box.y_max) - static_cast< double >(box.y_min));
  const double s_union = square * static_cast< double >(hits.hits_union) / static_cast< double >(tries);
  const double s_inter = square * static_cast< double >(hits.hits_inter) / static_cast< double >(tries);
  std::cout << std::setprecision(std::numeric_limits< double >::max_digits10) << s_union << ' ' << s_inter << '\n';
  return 0;
}

std::vector< sedov::Circle > sedov::readCircles(std::istream& in)
{
  std::vector< Circle > v;
  while (true) {
    long long r = 0, ignore = 0, x = 0, y = 0;
    if (!(in >> r)) {
      if (!in.eof()) {
        throw std::invalid_argument("Could not make out the figure");
      }
      break;
    }
    if (!(in >> ignore >> x >> y)) {
      throw std::invalid_argument("Incomplete figure");
    }
    v.emplace_back(Circle{r, x, y});
  }
  return v;
}

sedov::Box sedov::calcBox(const std::vector< Circle >& circles)
{
  const Circle& c = circles.front();
  Box b{c.x - c.r, c.x + c.r, c.y - c.r, c.y + c.r};
  for (size_t i = 1; i < circles.size(); ++i) {
    b.x_min = std::min(b.x_min, circles[i].x - circles[i].r);
    b.x_max = std::max(b.x_max, circles[i].x + circles[i].r);
    b.y_min = std::min(b.y_min, circles[i].y - circles[i].r);
    b.y_max = std::max(b.y_max, circles[i].y + circles[i].r);
  }
  return b;
}

bool sedov::isInside(double dx, double dy, double r)
{
  return dx * dx + dy * dy <= r * r;
}

sedov::Result sedov::calc(const std::vector< Circle >& circles, const Box& box, size_t tests, size_t seed)
{
  std::default_random_engine engine(seed);
  std::uniform_real_distribution< double > dist_x(static_cast< double >(box.x_min), static_cast< double >(box.x_max));
  std::uniform_real_distribution< double > dist_y(static_cast< double >(box.y_min), static_cast< double >(box.y_max));
  size_t hits_union = 0;
  size_t hits_inter = 0;
  for (size_t i = 0; i < tests; ++i) {
    const double x = dist_x(engine);
    const double y = dist_y(engine);
    bool in_any = false;
    bool in_all = true;
    for (size_t j = 0; j < circles.size(); ++j) {
      const bool res = isInside(x - static_cast< double >(circles[j].x), y - static_cast< double >(circles[j].y),
          static_cast< double >(circles[j].r));
      in_any = in_any || res;
      in_all = in_all && res;
    }
    if (in_any) {
      ++hits_union;
    }
    if (in_all) {
      ++hits_inter;
    }
  }
  return Result{hits_union, hits_inter};
}

sedov::Result sedov::countTotalHits(
    const std::vector< Circle >& circles, const Box& box, size_t threads, size_t tries, size_t seed)
{
  const size_t base = tries / threads;
  const size_t rem = tries % threads;
  std::vector< std::future< Result > > futures;
  futures.reserve(threads);
  for (size_t i = 0; i < threads; ++i) {
    size_t local_tries = base + (i < rem ? 1 : 0);
    size_t local_seed = seed + i;
    futures.emplace_back(
        std::async(std::launch::async, calc, std::cref(circles), std::cref(box), local_tries, local_seed));
  }
  Result res{0, 0};
  for (size_t i = 0; i < threads; ++i) {
    const Result local_res = futures[i].get();
    res.hits_union += local_res.hits_union;
    res.hits_inter += local_res.hits_inter;
  }
  return res;
}
