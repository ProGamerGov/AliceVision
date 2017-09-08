// This file is part of the AliceVision project and is made available under
// the terms of the MPL2 license (see the COPYING.md file).

#pragma once
#include "aliceVision/features/ImageDescriberCommon.hpp"
#include "aliceVision/features/image_describer.hpp"
#include "aliceVision/features/regions_factory.hpp"

namespace aliceVision {

namespace image {

template <typename T>
class Image;

} //namespace image

namespace features {

/**
 * @brief Create an Image_describer interface for OpenCV AKAZE feature extractor
 * Regions is the same as AKAZE floating point
 */
class AKAZE_openCV_ImageDescriber : public Image_describer
{
public:

  /**
   * @brief Get the corresponding EImageDescriberType
   * @return EImageDescriberType
   */
  virtual EImageDescriberType getDescriberType() const override
  {
    return EImageDescriberType::AKAZE_OCV;
  }

  /**
   * @brief Use a preset to control the number of detected regions
   * @param[in] preset The preset configuration
   * @return True if configuration succeed. (here always false)
   */
  bool Set_configuration_preset(EDESCRIBER_PRESET preset) override
  {
    return false;
  }

  /**
   * @brief Detect regions on the image and compute their attributes (description)
   * @param[in] image Image.
   * @param[out] regions The detected regions and attributes (the caller must delete the allocated data)
   * @param[in] mask 8-bit gray image for keypoint filtering (optional).
   *    Non-zero values depict the region of interest.
   * @return True if detection succed.
   */
  bool Describe(const image::Image<unsigned char>& image,
                std::unique_ptr<Regions> &regions,
                const image::Image<unsigned char> * mask = NULL) override;

  /**
   * @brief Allocate Regions type depending of the Image_describer
   * @param[in,out] regions
   */
  void Allocate(std::unique_ptr<Regions> &regions) const override
  {
    regions.reset( new AKAZE_Float_Regions );
  }

};

} //namespace features
} //namespace aliceVision
