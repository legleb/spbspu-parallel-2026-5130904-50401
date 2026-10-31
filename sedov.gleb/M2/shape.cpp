#include "shape.hpp"

#include <algorithm>
#include <future>
#include <random>
#include <vector>
#include <cstddef>
#include <functional>
#include <cmath>

sedov::Box sedov::boundingBox(const Shape& shape)
{
  if (shape.kind == sedov::Kind::Circle || shape.angle == 0.0) {
    return Box{shape.x - shape.a, shape.x + shape.a, shape.y - shape.b, shape.y + shape.b};
  }
  const double cos_a = std::cos(shape.angle);
  const double sin_a = std::sin(shape.angle);
  const double x_extent = std::sqrt(
      std::pow(static_cast< double >(shape.a) * cos_a, 2) + std::pow(static_cast< double >(shape.b) * sin_a, 2));
  const double y_extent = std::sqrt(
      std::pow(static_cast< double >(shape.a) * sin_a, 2) + std::pow(static_cast< double >(shape.b) * cos_a, 2));
  return Box{static_cast< long long >(std::floor(shape.x - x_extent)),
      static_cast< long long >(std::ceil(shape.x + x_extent)), static_cast< long long >(std::floor(shape.y - y_extent)),
      static_cast< long long >(std::ceil(shape.y + y_extent))};
}

sedov::Box sedov::boundingBox(const std::vector< Shape >& shapes)
{
  Box box = boundingBox(shapes.front());
  for (size_t i = 1; i < shapes.size(); ++i) {
    const Box b = boundingBox(shapes[i]);
    box.x_min = std::min(box.x_min, b.x_min);
    box.x_max = std::max(box.x_max, b.x_max);
    box.y_min = std::min(box.y_min, b.y_min);
    box.y_max = std::max(box.y_max, b.y_max);
  }
  return box;
}

bool sedov::isInside(double x, double y, const Shape& shape)
{
  const double dx = x - static_cast< double >(shape.x);
  const double dy = y - static_cast< double >(shape.y);
  const double cos_a = std::cos(-shape.angle);
  const double sin_a = std::sin(-shape.angle);
  const double tx = dx * cos_a - dy * sin_a;
  const double ty = dx * sin_a + dy * cos_a;
  const double nx = tx / static_cast< double >(shape.a);
  const double ny = ty / static_cast< double >(shape.b);
  return nx * nx + ny * ny <= 1.0;
}

sedov::Result sedov::calc(const std::vector< Shape >& shapes, const Box& box, size_t tests, size_t seed)
{
  std::default_random_engine engine(seed);
  const double width = static_cast< double >(box.x_max - box.x_min);
  const double height = static_cast< double >(box.y_max - box.y_min);
  const double x_min = static_cast< double >(box.x_min);
  const double y_min = static_cast< double >(box.y_min);
  const double max_val = static_cast< double >(engine.max());
  size_t hits = 0;
  for (size_t i = 0; i < tests; ++i) {
    const double x = x_min + (static_cast< double >(engine()) / max_val) * width;
    const double y = y_min + (static_cast< double >(engine()) / max_val) * height;
    for (size_t j = 0; j < shapes.size(); ++j) {
      if (isInside(x, y, shapes[j])) {
        ++hits;
        break;
      }
    }
  }
  return Result{hits};
}

sedov::Result sedov::countTotalHits(
    const std::vector< Shape >& shapes, const Box& box, size_t threads, size_t tries, size_t seed)
{
  if (threads == 1) {
    return calc(shapes, box, tries, seed);
  }
  const size_t base = tries / threads;
  const size_t rem = tries % threads;
  std::vector< std::future< Result > > futures;
  futures.reserve(threads);
  for (size_t i = 0; i < threads; ++i) {
    size_t local_tries = base + (i < rem ? 1 : 0);
    size_t local_seed = seed + i;
    futures.emplace_back(
        std::async(std::launch::async, calc, std::cref(shapes), std::cref(box), local_tries, local_seed));
  }
  Result res{0};
  for (size_t i = 0; i < threads; ++i) {
    const Result local_res = futures[i].get();
    res.hits += local_res.hits;
  }
  return res;
}
