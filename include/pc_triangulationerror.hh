#ifndef __PC_TRIANGULATIONERROR_HH__
#define __PC_TRIANGULATIONERROR_HH__

#include "pointclassifier.hh"

namespace sfmsimulator::pointclassifier {

class PC_Triangulationerror : Pointclassifier {
 public:
  const std::vector<bool> classify(
      const std::shared_ptr<points::Points2d> image_points_frame_1,
      const std::shared_ptr<points::Points2d> image_points_frame_2,
      const std::shared_ptr<points::Points3d> world_points_frame_1,
      const std::shared_ptr<points::Points3d> world_points_frame_2) const;

  void cluster(const points::Points2d image_points,
               const std::vector<bool> type) const;

 private:
};

}  // namespace sfmsimulator::pointclassifier

#endif /* end of include guard: __PC_TRIANGULATIONERROR_HH__ */
