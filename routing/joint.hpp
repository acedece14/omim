#pragma once

#include "routing/road_point.hpp"

#include "base/buffer_vector.hpp"

#include "std/cstdint.hpp"
#include "std/limits.hpp"
#include "std/string.hpp"

namespace routing
{
// Joint represents roads connection.
// It contains RoadPoint for each connected road.
class Joint final
{
public:
  using Id = uint32_t;
  static Id constexpr kInvalidId = numeric_limits<Id>::max();

  void AddPoint(RoadPoint const & rp) { m_points.emplace_back(rp); }

  size_t GetSize() const { return m_points.size(); }

  RoadPoint const & GetEntry(size_t i) const { return m_points[i]; }

private:
  buffer_vector<RoadPoint, 2> m_points;
};

string DebugPrint(Joint const & joint);
}  // namespace routing
