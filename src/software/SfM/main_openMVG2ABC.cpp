// This file is part of the AliceVision project and is made available under
// the terms of the MPL2 license (see the COPYING.md file).

#include <cstdlib>

// Command line
#include "third_party/cmdLine/cmdLine.h"

// AliceVision
#include "aliceVision/sfm/sfm.hpp"
#include "aliceVision/numeric/numeric.h"
#include "aliceVision/geometry/pose3.hpp"
using namespace aliceVision::sfm;

// Alembic
#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreHDF5/All.h>
using namespace Alembic::Abc;
namespace AbcG = Alembic::AbcGeom;
using namespace AbcG;

int main(int argc, char **argv) {
   
    // Get arguments
    CmdLine cmdLine;
    std::string sfmDataFilename;
    std::string sOutAlembic = "";

    cmdLine.add(make_option('i', sfmDataFilename, "sfmdata"));
    cmdLine.add(make_option('o', sOutAlembic, "outfile"));

    try {
        if (argc < 4) throw std::string("Invalid number of parameters in the command line.");
        cmdLine.process(argc, argv);
    } catch(const std::string &s) {
        std::cout << "aliceVision to alembic\n";
        std::cout << "Usage: " << argv[0] << '\n'
        << "[-i|--sfmdata filename, the SfM_Data file to convert]\n"
        << "[-o|--outfile path]\n"
        << std::endl;
        std::cerr << s << std::endl;
        return EXIT_FAILURE;
    }

    //
    SfM_Data sfm_data;
    if (!Load(sfm_data, sfmDataFilename, ESfM_Data(ALL))) {
        std::cout << "Error: The input project file \""
                  << sfmDataFilename << "\" cannot be read." << std::endl;
        return EXIT_FAILURE;
    }
  
    // Open Alembic archive
    // TODO: decide wether we want to keep HDF5 or switch to Ogawa 
    OArchive archive(Alembic::AbcCoreHDF5::WriteArchive(), sOutAlembic);
    OObject topObj(archive, kTop);

    //=================================================================================
    // Export points
    //=================================================================================

    std::vector<V3f> positions;
    positions.reserve(sfm_data.GetLandmarks().size());

    // For all the 3d points in the hash_map
    for(const auto lm : sfm_data.GetLandmarks()) {
        const aliceVision::Vec3 &pt = lm.second.X;
        positions.emplace_back(pt[0], pt[1], pt[2]);
    }

    std::vector<Alembic::Util::uint64_t> ids(positions.size());
    std::iota(std::begin(ids), std::end(ids), 0);

    OPoints partsOut(topObj, "particleShape1");
    OPointsSchema &pSchema = partsOut.getSchema();
    OPointsSchema::Sample psamp( std::move(V3fArraySample (positions)), std::move(UInt64ArraySample ( ids )));
    pSchema.set( psamp );

    //=================================================================================
    // Export cameras
    //=================================================================================
    for(const auto it: sfm_data.GetViews()) {
        const View * view = it.second.get();
        if (!sfm_data.IsPoseAndIntrinsicDefined(view))
          continue;

        // AliceVision Camera
        const aliceVision::geometry::Pose3 pose = sfm_data.getPose(*view);
        auto iterIntrinsic = sfm_data.GetIntrinsics().find(view->getIntrinsicId());
        aliceVision::cameras::Pinhole_Intrinsic *cam = static_cast<aliceVision::cameras::Pinhole_Intrinsic*>(iterIntrinsic->second.get());
        aliceVision::Mat34 P = cam->get_projective_equivalent(pose);

        // Extract internal matrix, rotation and translation
        //aliceVision::Mat3 R, K;
        //aliceVision::Vec3 t;
        //aliceVision::KRt_From_P(P, &K, &R, &t);
       
        const aliceVision::Mat3 R = pose.rotation();
        const aliceVision::Vec3 center = pose.center();

        // POSE
        // Compensate translation with rotation
        //aliceVision::Vec3 center = R.transpose() * t;
        // Build transform matrix
        Abc::M44d xformMatrix;
        xformMatrix[0][0]= R(0,0);
        xformMatrix[0][1]= R(0,1);
        xformMatrix[0][2]= R(0,2);
        xformMatrix[1][0]= R(1,0);
        xformMatrix[1][1]= R(1,1);
        xformMatrix[1][2]= R(1,2);
        xformMatrix[2][0]= R(2,0);
        xformMatrix[2][1]= R(2,1);
        xformMatrix[2][2]= R(2,2);
        xformMatrix[3][0]= center(0);
        xformMatrix[3][1]= center(1);
        xformMatrix[3][2]= center(2);
        xformMatrix[3][3]= 1.0;

        // Correct camera orientation for alembic
        M44d scale;
        scale[1][1] = -1;
        scale[2][2] = -1;
        
        xformMatrix = scale*xformMatrix;

        XformSample xformsample;
        xformsample.setMatrix(xformMatrix);

        stringstream ss;
        ss << stlplus::basename_part(view->getImagePath());
        Alembic::AbcGeom::OXform xform(topObj, "camxform_" + ss.str());
        xform.getSchema().set(xformsample);

        // Camera intrinsic parameters
        OCamera camObj(xform, "camera_" + ss.str());
        CameraSample camSample;
    
        // Take the max of the image size to handle the case where the image is in portrait mode 
        const float imgWidth = cam->w();  
        const float imgHeight = cam->h(); 
        const float sensorWidthPix = std::max(imgWidth, imgHeight);
        const float sensorHeightPix = std::min(imgWidth, imgHeight);
        const float focalLengthPix = cam->focal();
        const float dx = cam->principal_point()(0);
        const float dy = cam->principal_point()(1);
        // Use a common sensor width as we don't have this information at this point
        // We chose a full frame 24x36 camera
        const float sensorWidth = 36.0; // 36mm per default
        const float sensorHeight = sensorWidth*sensorHeightPix/sensorWidthPix;
        const float focalLength = sensorWidth*focalLengthPix/sensorWidthPix;
        // Following values are in cm, hence the 0.1 multiplier
        const float hoffset = 0.1*sensorWidth*(0.5-dx/imgWidth);
        const float voffset = 0.1*sensorHeight*(dy/imgHeight-0.5)*sensorHeightPix/sensorWidthPix;
        const float haperture = 0.1*sensorWidth*imgWidth/sensorWidthPix;
        const float vaperture = 0.1*sensorWidth*imgHeight/sensorWidthPix;

        camSample.setFocalLength(focalLength);
        camSample.setHorizontalAperture(haperture);
        camSample.setVerticalAperture(vaperture);
        camSample.setHorizontalFilmOffset(hoffset);
        camSample.setVerticalFilmOffset(voffset);

        camObj.getSchema().set( camSample );
    } 

    return EXIT_SUCCESS;
}
