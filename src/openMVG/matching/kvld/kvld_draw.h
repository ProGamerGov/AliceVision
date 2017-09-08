// This file is part of the AliceVision project and is made available under
// the terms of the MPL2 license (see the COPYING.md file).

#pragma once

#include "aliceVision/image/image.hpp"
#include "aliceVision/features/feature.hpp"
#include <vector>

namespace aliceVision {

//-- A slow but accurate way to draw K-VLD lines
void getKVLDMask(
  image::Image< unsigned char > *maskL,
  image::Image< unsigned char > *maskR,
  const std::vector< features::SIOPointFeature > &vec_F1,
  const std::vector< features::SIOPointFeature > &vec_F2,
  const std::vector< Pair >& vec_matches,
  const std::vector< bool >& vec_valide,
  const aliceVision::Mat& mat_E)
{
  for( int it1 = 0; it1 < vec_matches.size() - 1; it1++ )
  {
    for( int it2 = it1 + 1; it2 < vec_matches.size(); it2++ )
    {
      if( vec_valide[ it1 ] && vec_valide[ it2 ] && mat_E( it1, it2 ) >= 0 )
      {
        const features::SIOPointFeature & l1 = vec_F1[ vec_matches[ it1 ].first ];
        const features::SIOPointFeature & l2 = vec_F1[ vec_matches[ it2 ].first ];
        float l = ( l1.coords() - l2.coords() ).norm();
        int widthL = std::max( 1.f, l / ( dimension + 1.f ) );

        image::DrawLineThickness(l1.x(), l1.y(), l2.x(), l2.y(), 255, widthL, maskL);

        const features::SIOPointFeature & r1 = vec_F2[ vec_matches[ it1 ].second ];
        const features::SIOPointFeature & r2 = vec_F2[ vec_matches[ it2 ].second ];
        float r = ( r1.coords() - r2.coords() ).norm();
        int widthR = std::max( 1.f, r / ( dimension + 1.f ) );

        image::DrawLineThickness(r1.x(), r1.y(), r2.x(), r2.y(), 255, widthR, maskR);
      }
    }
  }
}

}; // namespace aliceVision
