#ifndef _FRONT_BACK_BOUNDARY_ITERATOR_H_
#define _FRONT_BACK_BOUNDARY_ITERATOR_H_

#include "ParallelBoundaryIterator.h"

template <typename FF, typename T>
class FrontBackBoundaryIterator : public ParallelBoundaryIterator<FF, T> {
 public:
  FrontBackBoundaryIterator(
      FF& flowField, const Parameters& parameters, Point lowOffset,
      Point highOffset,
      std::function<void(FF& flowField, int, int, int, T&)> apply)
      : FrontBackBoundaryIterator(
            flowField, parameters, lowOffset,
            ParallelBoundaryIterator<FF, T>::highOffsetToPoint(flowField,
                                                               highOffset),
            apply, false) {}

 private:
  FrontBackBoundaryIterator(
      FF& flowField, const Parameters& parameters, Point low, Point high,
      std::function<void(FF& flowField, int, int, int, T&)> apply, bool x)
      : ParallelBoundaryIterator<FF, T>(
            flowField, parameters, low, Point(high.x, high.y, low.z),
            Point(low.x, low.y, high.z), high, apply){};
};

#endif  // _FRONT_BACK_BOUNDARY_ITERATOR_H_