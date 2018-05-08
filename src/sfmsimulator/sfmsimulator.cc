#include "bundleadjustment.hh"
#include "pc_reprojectionerror.hh"
#include "sfmsimulator.hh"

#include <random>

#include <opencv2/core/affine.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/core/eigen.hpp>
#include <opencv2/sfm.hpp>

#include <cassert>

namespace sfmsimulator {

Sfmsimulator::Sfmsimulator(Sfmconfig config)
    : _config(config), _cameramodel(config.cameramodel),
      _framesimulator(framesimulator::Framesimulator(
          config.filepaths, config.cameramodel, config.noise_image_detection)) {

  std::cout
      << "\033[0m\n"
      << "\n\n\n\n"
      << "            __\n"
      << "     ___   / _|  _ __ ___      \n"
      << "    / __| | |_  | '_ ` _ \\     \n"
      << "    \\__ \\ |  _| | | | | | |     \n"
      << "    |___/ |_|   |_| |_| |_|   \n\n"
      << "           _                       _           _\n"
      << "     ___  (_)  _ __ ___    _   _  | |   __ _  | |_    ___    _ __ "
      << "\n    / __| | | | '_ ` _ \\  | | | | | |  / _` | | __|  / _ \\  | "
      << "'__|\n"
      << "    \\__ \\ | | | | | | | | | |_| | | | | (_| | | |_  | (_) | | |  "
      << " \n"
      << "    |___/ |_| |_| |_| |_|  \\__,_| |_|  \\__,_|  \\__|  \\___/  "
      << "|_| \n\n\n\n"
      << "    author: david schmidig [david@davencyw.net]\n"
      << "            davencyw code  [davencyw.net]\n"
      << "            ETH Zurich\n\n"
      << "___________________________________________________________________"
         "__"
         "\n\n\n";

  using pct = pointclassifier::Pointclassifier_type;

  switch (config.type_pointclassifier) {
  case pct::PC_ReprojectionErrorDep1_t:
    _pointclassifier =
        std::make_unique<pointclassifier::PC_ReprojectionErrorDep1>(
            pointclassifier::PC_ReprojectionErrorDep1());
    break;
  case pct::PC_ReprojectionErrorDep2_t:
    _pointclassifier =
        std::make_unique<pointclassifier::PC_ReprojectionErrorDep2>(
            pointclassifier::PC_ReprojectionErrorDep2());
  case pct::PC_ReprojectionErrorNodep_t:
    _pointclassifier =
        std::make_unique<pointclassifier::PC_ReprojectionErrorNodep>(
            pointclassifier::PC_ReprojectionErrorNodep());
  default:
    std::cout << "No classifier defined!!\n\n";
    break;
  }

  if (config.filepaths.size() > 3) {
    _file_output = config.filepaths[3];
  }
  _fstream_output_weights = std::make_unique<std::ofstream>();
  _fstream_output_camera_trajectory = std::make_unique<std::ofstream>();
  _fstream_output_weights->open(_file_output + "_weights.csv");
  _fstream_output_camera_trajectory->open(_file_output + "_camera.csv");
}

void Sfmsimulator::run() {
  const size_t steps(_framesimulator.updatesLeft());
  std::cout << "STEPS: " << steps << "\n\n\n";

  for (size_t weight_i(0); weight_i < static_cast<size_t>(_weights.size());
       ++weight_i) {
    *_fstream_output_weights << _weights(weight_i) << ",";
  }
  *_fstream_output_weights << "\n";

  for (size_t step_i(0); step_i < steps; step_i++) {
    step();
  }
}

// TODO(dave) add generic depending sliding window to pc_classifier
// TODO(dave) separate initializationsteps (_steps=0,1) from this method
void Sfmsimulator::step() {
  std::cout << " - STEP[ " << _step << " ]\n";

  _framesimulator.step();

  _scene_window_image.push_back(
      std::make_shared<points::Points2d>(_framesimulator.getImagePoints()));
  _scene_window_cameraposes.push_back(_framesimulator.getCameraPose());
  _scene_window_world.push_back(
      std::make_shared<points::Points3d>(_framesimulator.getWorldPoints()));

  const size_t numpoints(_scene_window_image[0]->numpoints);

  // initialization step
  if (_scene_window_image.size() < 2) {
    assert(_step == 0);
    _weights = array_t::Ones(numpoints);
    ++_step;
    return;
  }

  assert(_scene_window_image.size() == 2);

  std::cout << " -    reconstruction \n";

  std::vector<std::shared_ptr<points::Points2d>> frames;
  std::vector<vec6_t> cameraposes;

  // convert from deque to vector
  for (auto &frame_i : _scene_window_image) {
    frames.push_back(frame_i);
  }
  // have to bemutable for BA, so copy into new vector
  for (auto &pose_i : _scene_window_cameraposes) {
    cameraposes.push_back(pose_i);
  }

  // add noise to ground truth
  if (_config.noise_camera || _config.noise_3dposition) {
    addNoise(_scene_window_world.front(), cameraposes, 0.3);
  }

  Sfmreconstruction reconstruct = bundleadjustment::adjustBundle(
      frames, _scene_window_world.front(), cameraposes, _cameramodel, _weights);

  _scene_window_world_estimate.push_front(reconstruct.point3d_estimate);
  _scene_window_cameraposes_mat.push_front(
      reconstruct.camerapose_estimate_mat[1]);

  if (_pointclassifier) {
    // classify and reconstruct with only static points
    _pointclassifier->classifynext(reconstruct, _weights);
    std::cout << " -    classify \n";
  }

  output(reconstruct);

  _scene_window_world_estimate.pop_back();
  _scene_window_world.pop_front();
  _scene_window_image.pop_front();
  _scene_window_cameraposes.pop_front();
  ++_step;
}

void Sfmsimulator::addNoise(std::shared_ptr<points::Points3d> points,
                            std::vector<vec6_t> cameraposes,
                            precision_t amount) {

  //  add noise to worldpoints and camerapose
  if (amount == 0) {
    return;
  }

  std::random_device rd{};
  std::mt19937 gen{rd()};
  std::normal_distribution<> d{0, amount};

  if (_config.noise_3dposition) {
    const size_t numpoints(points->coord.size());
    for (size_t point_i(0); point_i < numpoints; ++point_i) {
      precision_t random(d(gen));
      points->coord[0] += random;
      random = d(gen);
      points->coord[1] += random;
      // no z noise for the boyz
      // random = d(gen);
      // points->coord[2] += random;
    }
  }
  if (_config.noise_camera) {
    for (size_t param_i(0); param_i < 6; ++param_i) {
      const precision_t random(d(gen));
      _scene_window_cameraposes.front()(param_i) += random;
    }
  }
}

void Sfmsimulator::output(const Sfmreconstruction &reconstruct) const {
  for (size_t weight_i(0); weight_i < static_cast<size_t>(_weights.size());
       ++weight_i) {
    *_fstream_output_weights << _weights(weight_i) << ",";
  }
  *_fstream_output_weights << "\n";

  for (size_t coeff_i(0); coeff_i < 6; ++coeff_i) {
    // ground truth
    *_fstream_output_camera_trajectory
        << (_scene_window_cameraposes.back())(coeff_i) << " ";
  }
  *_fstream_output_camera_trajectory << "\n";

  auto camerapose =
      reconstruct
          .camerapose_estimate[reconstruct.camerapose_estimate.size() - 1];
  for (size_t coeff_i(0); coeff_i < 6; ++coeff_i) {
    // estimate
    *_fstream_output_camera_trajectory << camerapose(coeff_i) << " ";
  }
  *_fstream_output_camera_trajectory << "\n";
}

} // namespace sfmsimulator
