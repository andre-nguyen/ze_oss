#pragma once

#include <ze/common/types.h>

namespace ze {

//! Generate count random visible keypoints.
Keypoints generateRandomKeypoints(
    const int image_width, const int image_height, const int margin, const size_t count);

//! Return if pixel u is within image boundaries.
template<typename DerivedKeyPoint>
bool isVisible(
    const typename DerivedKeyPoint::Scalar image_width,
    const typename DerivedKeyPoint::Scalar image_height,
    const Eigen::MatrixBase<DerivedKeyPoint>& px)
{
  return px[0] >= 0
      && px[1] >= 0
      && px[0] <  image_width
      && px[1] <  image_height;
}

//! Return if pixel px is within image boundaries with margin.
template<typename DerivedKeyPoint>
bool isVisibleWithMargin(
    const typename DerivedKeyPoint::Scalar image_width,
    const typename DerivedKeyPoint::Scalar image_height,
    const Eigen::MatrixBase<DerivedKeyPoint>& px,
    const typename DerivedKeyPoint::Scalar margin)
{
  return px[0] >= margin
      && px[1] >= margin
      && px[0] < (image_width - margin)
      && px[1] < (image_height - margin);
}

//! Return if pixel px is within image boundaries with margin.
inline bool isVisibleWithMargin(
    const int image_width, const int image_height, const int x, const int y, const int margin)
{
  return x >= margin
      && y >= margin
      && x < (image_width - margin)
      && y < (image_height - margin);
}

} // namespace ze
