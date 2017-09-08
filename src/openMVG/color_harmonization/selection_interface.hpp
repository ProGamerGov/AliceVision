// This file is part of the AliceVision project and is made available under
// the terms of the MPL2 license (see the COPYING.md file).

#ifndef OPENMVG_COLORHARMONIZATION_SELECTORINTERFACE_H
#define OPENMVG_COLORHARMONIZATION_SELECTORINTERFACE_H

#include "aliceVision/image/image.hpp"
#include "third_party/histogram/histogram.hpp"
#include <string>

namespace aliceVision {
namespace color_harmonization {

using namespace std;

class commonDataByPair
{
public:
  commonDataByPair( const std::string & sLeftImage,
                    const std::string & sRightImage ):
    _sLeftImage( sLeftImage ), _sRightImage( sRightImage )
  {}

  virtual ~commonDataByPair() {}

  /**
   * Compute mask forthe two images
   *
   * \param[out] maskLeft Mask of the left image (initialized to corresponding image size).
   * \param[out] maskRight  Mask of the right image (initialized to corresponding image size).
   *
   * \return True if(mask not empty).
   */
  virtual bool computeMask( image::Image< unsigned char > & maskLeft, image::Image< unsigned char > & maskRight ) = 0;

  /**
   * Compute Histogram for the color's masked data
   *
   * \param[in] mask Binary image to determine acceptable zones
   * \param[in] channelIndex selected channel : 0 = red; 1 = green; 2 = blue
   * \param[in] image Image with RGB or LAB type
   * \param[out] histo  Histogram of the left image.
   *
   */
  template< typename ImageType >
  static void computeHisto(
    Histogram< double > & histo,
    const image::Image< unsigned char >& mask,
    size_t channelIndex,
    const image::Image< ImageType >& image )
  {
    for( int j = 0; j < mask.Height(); ++j )
    {
      for( int i = 0; i < mask.Width(); ++i )
      {
        if( ( int )mask( j, i ) != 0 )
        {
          histo.Add( image( j, i )( channelIndex ) );
        }
      }
    }
  }

  const std::string & getLeftImage()const{ return _sLeftImage; }
  const std::string & getRightImage()const{ return _sRightImage; }

protected:
  // Left and Right image filenames
  std::string _sLeftImage, _sRightImage;
};

}  // namespace color_harmonization
}  // namespace aliceVision

#endif // OPENMVG_COLORHARMONIZATION_SELECTORINTERFACE_H
