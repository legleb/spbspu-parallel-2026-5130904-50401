#include <iostream>

namespace sedov
{
  struct Circle
  {
    long long r, x, y;
  };

  std::vector< Circle > read_circles(std::istream & in);

  struct Box
  {
    long long x_min, x_max, y_min, y_max;
  };

  Box calc_box(const std::vector< Circle > & circles);
}

int main(int argc, char ** argv)
{
  long long threads = 0, tries = 0, seed = 0;
  if (argc < 3 || argc > 4) {
    std::cerr << "Аргументов слишком мало\n";
    return 1;
  }
  try {
    threads = std::stoll(argv[1]);
    tries = std::stoi(argv[2]);
    if (argc == 4) {
      seed = std::stoi(argv[3]);
    }
  } catch (const std::invalid_argument & ia) {
    std::cerr << ia.what() << '\n';
    return 1;
  } catch (const std::out_of_range & oor) {
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
    circles = sedov::read_circles(std::cin);
  } catch (const std::invalid_argument & ia) {
    std::cerr << ia.what() << '\n';
    return 2;
  } catch (const std::bad_alloc & ba) {
    std::cerr << ba.what() << '\n';
    return 2;
  }
  if (circles.empty()) {
    std::cout << 0 << ' ' << 0 << '\n';
    return 0;
  }

  sedov::Box box = sedov::calc_box(circles);
}

std::vector< sedov::Circle > sedov::read_circles(std::istream & in)
{
  std::vector< Circle > v;
  while (true) {
    long long r = 0, ignore = 0, x = 0, y = 0;
    if (!(in >> r)) {
      if (!in.eof()) {
        throw std::invalid_argument("Не удалось разобрать фигуру");
      }
      break;
    }
    if (!(in >> ignore >> x >> y)) {
      throw std::invalid_argument("Неполная фигура");
    }
    v.emplace_back(Circle{r, x, y});
  }
  return v;
}

sedov::Box sedov::calc_box(const std::vector< Circle > & circles)
{
  const Circle & c = circles.front();
  Box b{c.x - c.r, c.x + c.r, c.y - c.r, c.y + c.r};
  for (size_t i = 1; i < circles.size(); ++i) {
    b.x_min = std::min(b.x_min, circles[i].x - circles[i].r);
    b.x_max = std::max(b.x_max, circles[i].x + circles[i].r);
    b.y_min = std::min(b.y_min, circles[i].y - circles[i].r);
    b.y_max = std::max(b.y_max, circles[i].y + circles[i].r);
  }
  return b;
}
