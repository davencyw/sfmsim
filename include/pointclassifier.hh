#ifndef __POINTCLASSIFIER_HH__
#define __POINTCLASSIFIER_HH__

namespace sfmsimulator::pointclassifier {

class Pointclassifier {
 public:
  virtual void classify() = 0;
  virtual void cluster() = 0;

 private:
};

}  // namespace sfmsimulator::pointclassifier

#endif /* end of include guard: __POINTCLASSIFIER_HH__ */
