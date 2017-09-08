// This file is part of the AliceVision project and is made available under
// the terms of the MPL2 license (see the COPYING.md file).

#include "aliceVision/sfm/sfm.hpp"
#include "aliceVision/image/image.hpp"

using namespace aliceVision;
using namespace aliceVision::cameras;
using namespace aliceVision::geometry;
using namespace aliceVision::image;
using namespace aliceVision::sfm;

#include "third_party/cmdLine/cmdLine.h"
#include "third_party/stlplus3/filesystemSimplified/file_system.hpp"
#include "aliceVision/numeric/numeric.h"

#include <fstream>

int main(int argc, char **argv)
{
  CmdLine cmd;

  std::string sSfM_Data_Filename;
  std::string sPly = "";
  std::string sOutDir = "";

  cmd.add( make_option('i', sSfM_Data_Filename, "sfmdata") );
  cmd.add( make_option('p', sPly, "ply") );
  cmd.add( make_option('o', sOutDir, "outdir") );

  try {
      if (argc == 1) throw std::string("Invalid command line parameter.");
      cmd.process(argc, argv);
  } catch(const std::string& s) {
      std::cerr << "Usage: " << argv[0] << '\n'
      << "[-i|--sfmdata] filename, the SfM_Data file to convert\n"
      << "[-p|--ply] path\n"
      << "[-o|--outdir] path\n"
      << std::endl;

      std::cerr << s << std::endl;
      return EXIT_FAILURE;
  }

  std::cout << " You called : " <<std::endl
            << argv[0] << std::endl
            << "--sfmdata " << sSfM_Data_Filename << std::endl
            << "--ply " << sPly << std::endl
            << "--outdir " << sOutDir << std::endl;

  // Create output dir
  if (!stlplus::folder_exists(sOutDir))
    stlplus::folder_create( sOutDir );

  // Read the SfM scene
  SfM_Data sfm_data;
  if (!Load(sfm_data, sSfM_Data_Filename, ESfM_Data(VIEWS|INTRINSICS|EXTRINSICS))) {
    std::cerr << std::endl
      << "The input SfM_Data file \""<< sSfM_Data_Filename << "\" cannot be read." << std::endl;
    return EXIT_FAILURE;
  }

  std::ofstream outfile( stlplus::create_filespec( sOutDir, "sceneMeshlab", "mlp" ).c_str() );

  // Init mlp file
  outfile << "<!DOCTYPE MeshLabDocument>" << outfile.widen('\n')
    << "<MeshLabProject>" << outfile.widen('\n')
    << " <MeshGroup>" << outfile.widen('\n')
    << "  <MLMesh label=\"" << sPly << "\" filename=\"" << sPly << "\">" << outfile.widen('\n')
    << "   <MLMatrix44>" << outfile.widen('\n')
    << "1 0 0 0 " << outfile.widen('\n')
    << "0 1 0 0 " << outfile.widen('\n')
    << "0 0 1 0 " << outfile.widen('\n')
    << "0 0 0 1 " << outfile.widen('\n')
    << "</MLMatrix44>" << outfile.widen('\n')
    << "  </MLMesh>" << outfile.widen('\n')
    << " </MeshGroup>" << outfile.widen('\n');

  outfile <<  " <RasterGroup>" << outfile.widen('\n');

  for(Views::const_iterator iter = sfm_data.GetViews().begin();
      iter != sfm_data.GetViews().end(); ++iter)
  {
    const View * view = iter->second.get();
    if (!sfm_data.IsPoseAndIntrinsicDefined(view))
      continue;

    const Pose3 pose = sfm_data.getPose(*view);
    Intrinsics::const_iterator iterIntrinsic = sfm_data.GetIntrinsics().find(view->getIntrinsicId());

    // We have a valid view with a corresponding camera & pose
    const std::string srcImage = stlplus::create_filespec(sfm_data.s_root_path, view->getImagePath());
    const IntrinsicBase * cam = iterIntrinsic->second.get();
    Mat34 P = cam->get_projective_equivalent(pose);

    for ( int i = 1; i < 3 ; ++i)
      for ( int j = 0; j < 4; ++j)
        P(i, j) *= -1.;

    Mat3 R, K;
    Vec3 t;
    KRt_From_P( P, &K, &R, &t);

    const Vec3 optical_center = R.transpose() * t;

    outfile
      << "  <MLRaster label=\"" << stlplus::filename_part(view->getImagePath()) << "\">" << std::endl
      << "   <VCGCamera TranslationVector=\""
      << optical_center[0] << " "
      << optical_center[1] << " "
      << optical_center[2] << " "
      << " 1 \""
      << " LensDistortion=\"0 0\""
      << " ViewportPx=\"" << cam->w() << " " << cam->h() << "\""
      << " PixelSizeMm=\"" << 1  << " " << 1 << "\""
      << " CenterPx=\"" << cam->w() / 2.0 << " " << cam->h() / 2.0 << "\""
      << " FocalMm=\"" << (double)K(0, 0 )  << "\""
      << " RotationMatrix=\""
      << R(0, 0) << " " << R(0, 1) << " " << R(0, 2) << " 0 "
      << R(1, 0) << " " << R(1, 1) << " " << R(1, 2) << " 0 "
      << R(2, 0) << " " << R(2, 1) << " " << R(2, 2) << " 0 "
      << "0 0 0 1 \"/>"  << std::endl;

    // Link the image plane
    outfile << "   <Plane semantic=\"\" fileName=\"" << srcImage << "\"/> "<< std::endl;
    outfile << "  </MLRaster>" << std::endl;
  }
  outfile << "   </RasterGroup>" << std::endl
    << "</MeshLabProject>" << std::endl;

  outfile.close();

  return EXIT_SUCCESS;
}
