// This file is part of the AliceVision project and is made available under
// the terms of the MPL2 license (see the COPYING.md file).

#ifndef OPENMVG_MULTIVIEW_TRANSLATION_AVERAGING_COMMON_H_
#define OPENMVG_MULTIVIEW_TRANSLATION_AVERAGING_COMMON_H_

#include "aliceVision/types.hpp"
#include "aliceVision/numeric/numeric.h"

#include <utility>
#include <vector>

namespace aliceVision {

/// Relative information [Rij|tij] for a pair
typedef std::pair< Pair, std::pair<Mat3,Vec3> > relativeInfo;

typedef std::vector< relativeInfo > RelativeInfo_Vec;
typedef std::map< Pair, std::pair<Mat3, Vec3> > RelativeInfo_Map;

// List the pairs used by the relative motions
static Pair_Set getPairs(const RelativeInfo_Vec & vec_relative)
{
  Pair_Set pair_set;
  for(size_t i = 0; i < vec_relative.size(); ++i)
  {
    const relativeInfo & rel = vec_relative[i];
    pair_set.insert(Pair(rel.first.first, rel.first.second));
  }
  return pair_set;
}

// List the index used by the relative motions
static std::set<IndexT> getIndexT(const RelativeInfo_Vec & vec_relative)
{
  std::set<IndexT> indexT_set;
  for (RelativeInfo_Vec::const_iterator iter = vec_relative.begin();
    iter != vec_relative.end(); ++iter)
  {
    indexT_set.insert(iter->first.first);
    indexT_set.insert(iter->first.second);
  }
  return indexT_set;
}


} // namespace aliceVision

#endif //OPENMVG_MULTIVIEW_TRANSLATION_AVERAGING_COMMON_H_
