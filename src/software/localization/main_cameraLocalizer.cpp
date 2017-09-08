// This file is part of the AliceVision project and is made available under
// the terms of the MPL2 license (see the COPYING.md file).

#include <aliceVision/config.hpp>
#include <aliceVision/localization/ILocalizer.hpp>
#include <aliceVision/localization/VoctreeLocalizer.hpp>
#if OPENMVG_IS_DEFINED(OPENMVG_HAVE_CCTAG)
#include <aliceVision/localization/CCTagLocalizer.hpp>
#endif
#include <aliceVision/localization/LocalizationResult.hpp>
#include <aliceVision/localization/optimization.hpp>
#include <aliceVision/image/image_io.hpp>
#include <aliceVision/dataio/FeedProvider.hpp>
#include <aliceVision/features/image_describer.hpp>
#include <aliceVision/features/ImageDescriberCommon.hpp>
#include <aliceVision/robust_estimation/robust_estimators.hpp>
#include <aliceVision/system/Logger.hpp>

#include <boost/filesystem.hpp>
#include <boost/progress.hpp>
#include <boost/program_options.hpp> 
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/sum.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <memory>

#if OPENMVG_IS_DEFINED(OPENMVG_HAVE_ALEMBIC)
#include <aliceVision/sfm/AlembicExporter.hpp>
#endif // OPENMVG_HAVE_ALEMBIC


namespace bfs = boost::filesystem;
namespace bacc = boost::accumulators;
namespace po = boost::program_options;

using namespace aliceVision;


std::string myToString(std::size_t i, std::size_t zeroPadding)
{
  std::stringstream ss;
  ss << std::setw(zeroPadding) << std::setfill('0') << i;
  return ss.str();
}

/**
 * @brief It checks if the value for the reprojection error or the matching error
 * is compatible with the given robust estimator. The value cannot be 0 for 
 * LORansac, for ACRansac a value of 0 means to use infinity (ie estimate the 
 * threshold during ransac process)
 * @param e The estimator to be checked.
 * @param value The value for the reprojection or matching error.
 * @return true if the value is compatible
 */
bool checkRobustEstimator(robust::EROBUST_ESTIMATOR e, double &value)
{
  if(e != robust::EROBUST_ESTIMATOR::ROBUST_ESTIMATOR_LORANSAC &&
     e != robust::EROBUST_ESTIMATOR::ROBUST_ESTIMATOR_ACRANSAC)
  {
    OPENMVG_CERR("Only " << robust::EROBUST_ESTIMATOR::ROBUST_ESTIMATOR_ACRANSAC 
            << " and " << robust::EROBUST_ESTIMATOR::ROBUST_ESTIMATOR_LORANSAC
            << " are supported.");
    return false;
  }
  if(value == 0 && 
     e == robust::EROBUST_ESTIMATOR::ROBUST_ESTIMATOR_ACRANSAC)
  {
    // for acransac set it to infinity
    value = std::numeric_limits<double>::infinity();
  }
  // for loransac we need thresholds > 0
  if(e == robust::EROBUST_ESTIMATOR::ROBUST_ESTIMATOR_LORANSAC)
  {
    const double minThreshold = 1e-6;
    if( value <= minThreshold)
    {
      OPENMVG_CERR("Error: errorMax and matchingError cannot be 0 with " 
              << robust::EROBUST_ESTIMATOR::ROBUST_ESTIMATOR_LORANSAC 
              << " estimator.");
      return false;     
    }
  }

  return true;
}

int main(int argc, char** argv)
{
  /// the calibration file
  std::string calibFile;
  /// the AliceVision .json data file
  std::string sfmFilePath;
  /// the folder containing the descriptors
  std::string descriptorsFolder;
  /// the media file to localize
  std::string mediaFilepath;
 
  /// the describer types name to use for the matching
  std::string matchDescTypeNames = features::EImageDescriberType_enumToString(features::EImageDescriberType::SIFT);
  /// the preset for the feature extractor
  features::EDESCRIBER_PRESET featurePreset = features::EDESCRIBER_PRESET::NORMAL_PRESET;     
  /// the describer types to use for the matching
  std::vector<features::EImageDescriberType> matchDescTypes;
  /// the estimator to use for resection
  robust::EROBUST_ESTIMATOR resectionEstimator = robust::EROBUST_ESTIMATOR::ROBUST_ESTIMATOR_ACRANSAC;        
  /// the estimator to use for matching
  robust::EROBUST_ESTIMATOR matchingEstimator = robust::EROBUST_ESTIMATOR::ROBUST_ESTIMATOR_ACRANSAC;        
  /// the possible choices for the estimators as strings
  const std::string str_estimatorChoices = ""+robust::EROBUST_ESTIMATOR_enumToString(robust::EROBUST_ESTIMATOR::ROBUST_ESTIMATOR_ACRANSAC)
                                          +","+robust::EROBUST_ESTIMATOR_enumToString(robust::EROBUST_ESTIMATOR::ROBUST_ESTIMATOR_LORANSAC);
  bool refineIntrinsics = false;
  /// the maximum reprojection error allowed for resection
  double resectionErrorMax = 4.0;  
  /// the maximum reprojection error allowed for image matching with geometric validation
  double matchingErrorMax = 4.0;   
  /// whether to use the voctreeLocalizer or cctagLocalizer
  bool useVoctreeLocalizer = true;
  
  // voctree parameters
  std::string algostring = "AllResults";
  /// number of similar images to search when querying the voctree
  std::size_t numResults = 4;
  /// maximum number of successfully matched similar images
  std::size_t maxResults = 10;      
  std::size_t numCommonViews = 3;
  /// the vocabulary tree file
  std::string vocTreeFilepath;
  /// the vocabulary tree weights file
  std::string weightsFilepath;
  /// Number of previous frame of the sequence to use for matching
  std::size_t nbFrameBufferMatching = 10;
  /// enable/disable the robust matching (geometric validation) when matching query image
  /// and databases images
  bool robustMatching = true;
  
  /// the Alembic export file
  std::string exportAlembicFile = "trackedcameras.abc";
  /// the Binary export file
  std::string exportBinaryFile = "";

#if OPENMVG_IS_DEFINED(OPENMVG_HAVE_CCTAG)
  // parameters for cctag localizer
  std::size_t nNearestKeyFrames = 5;   
#endif
  // parameters for the final bundle adjustment
  /// If !refineIntrinsics it can run a final global bundle to refine the scene
  bool globalBundle = false;
  /// It does not count the distortion
  bool noDistortion = false;
  /// It does not refine intrinsics during BA
  bool noBArefineIntrinsics = false;
  /// remove the points that does not have a minimum visibility over the sequence
  /// ie that are seen at least by minPointVisibility frames of the sequence
  std::size_t minPointVisibility = 0;
  
  /// whether to save visual debug info
  std::string visualDebug = "";

  po::options_description allParams(
      "This program takes as input a media (image, image sequence, video) and a database (vocabulary tree, 3D scene data) \n"
      "and returns for each frame a pose estimation for the camera.");

  po::options_description inputParams("Required input parameters");
  
  inputParams.add_options()
      ("sfmdata", po::value<std::string>(&sfmFilePath)->required(), 
          "The sfm_data.json kind of file generated by AliceVision.")
      ("mediafile", po::value<std::string>(&mediaFilepath)->required(), 
          "The folder path or the filename for the media to track");
  
  po::options_description commonParams(
      "Common optional parameters for the localizer");
  commonParams.add_options()
      ("descriptorPath", po::value<std::string>(&descriptorsFolder),
          "Folder containing the descriptors for all the images (ie the *.desc.)")
      ("matchDescTypes", po::value<std::string>(&matchDescTypeNames)->default_value(matchDescTypeNames),
          "The describer types to use for the matching")
      ("preset", po::value<features::EDESCRIBER_PRESET>(&featurePreset)->default_value(featurePreset), 
          "Preset for the feature extractor when localizing a new image "
          "{LOW,MEDIUM,NORMAL,HIGH,ULTRA}")
      ("resectionEstimator", po::value<robust::EROBUST_ESTIMATOR>(&resectionEstimator)->default_value(resectionEstimator), 
          std::string("The type of *sac framework to use for resection "
          "{"+str_estimatorChoices+"}").c_str())
      ("matchingEstimator", po::value<robust::EROBUST_ESTIMATOR>(&matchingEstimator)->default_value(matchingEstimator), 
          std::string("The type of *sac framework to use for matching "
          "{"+str_estimatorChoices+"}").c_str())
      ("calibration", po::value<std::string>(&calibFile)/*->required( )*/, 
          "Calibration file")
      ("refineIntrinsics", po::bool_switch(&refineIntrinsics), 
          "Enable/Disable camera intrinsics refinement for each localized image")
      ("reprojectionError", po::value<double>(&resectionErrorMax)->default_value(resectionErrorMax), 
          "Maximum reprojection error (in pixels) allowed for resectioning. If set "
          "to 0 it lets the ACRansac select an optimal value.");
  
// voctree specific options
  po::options_description voctreeParams("Parameters specific for the vocabulary tree-based localizer");
  voctreeParams.add_options()
      ("nbImageMatch", po::value<std::size_t>(&numResults)->default_value(numResults), 
          "[voctree] Number of images to retrieve in database")
      ("maxResults", po::value<std::size_t>(&maxResults)->default_value(maxResults), 
          "[voctree] For algorithm AllResults, it stops the image matching when "
          "this number of matched images is reached. If 0 it is ignored.")
      ("commonviews", po::value<std::size_t>(&numCommonViews)->default_value(numCommonViews), 
          "[voctree] Number of minimum images in which a point must be seen to "
          "be used in cluster tracking")
      ("voctree", po::value<std::string>(&vocTreeFilepath), 
          "[voctree] Filename for the vocabulary tree")
      ("voctreeWeights", po::value<std::string>(&weightsFilepath), 
          "[voctree] Filename for the vocabulary tree weights")
      ("algorithm", po::value<std::string>(&algostring)->default_value(algostring), 
          "[voctree] Algorithm type: FirstBest, AllResults" )
      ("matchingError", po::value<double>(&matchingErrorMax)->default_value(matchingErrorMax), 
          "[voctree] Maximum matching error (in pixels) allowed for image matching with "
          "geometric verification. If set to 0 it lets the ACRansac select "
          "an optimal value.")
      ("nbFrameBufferMatching", po::value<std::size_t>(&nbFrameBufferMatching)->default_value(nbFrameBufferMatching),
          "[voctree] Number of previous frame of the sequence to use for matching "
          "(0 = Disable)")
      ("robustMatching", po::value<bool>(&robustMatching)->default_value(robustMatching), 
          "[voctree] Enable/Disable the robust matching between query and database images, "
          "all putative matches will be considered.")
// cctag specific options
#if OPENMVG_IS_DEFINED(OPENMVG_HAVE_CCTAG)
      ("nNearestKeyFrames", po::value<size_t>(&nNearestKeyFrames)->default_value(nNearestKeyFrames), 
          "[cctag] Number of images to retrieve in the database")
#endif
  ;
  
// final bundle adjustment options
  po::options_description bundleParams("Parameters specific for final (optional) bundle adjustment optimization of the sequence");
  bundleParams.add_options()
      ("globalBundle", po::bool_switch(&globalBundle), 
          "[bundle adjustment] If --refineIntrinsics is not set, this option "
          "allows to run a final global budndle adjustment to refine the scene")
      ("noDistortion", po::bool_switch(&noDistortion), 
          "[bundle adjustment] It does not take into account distortion during "
          "the BA, it consider the distortion coefficients all equal to 0")
      ("noBArefineIntrinsics", po::bool_switch(&noBArefineIntrinsics), 
          "[bundle adjustment] It does not refine intrinsics during BA")
      ("minPointVisibility", po::value<size_t>(&minPointVisibility)->default_value(minPointVisibility), 
          "[bundle adjustment] Minimum number of observation that a point must "
          "have in order to be considered for bundle adjustment");
  
// output options
  po::options_description outputParams("Options for the output of the localizer");
  outputParams.add_options()
      ("help,h", "Print this message")
      ("visualDebug", po::value<std::string>(&visualDebug), 
          "If a directory is provided it enables visual debug and saves all the "
          "debugging info in that directory")

#if OPENMVG_IS_DEFINED(OPENMVG_HAVE_ALEMBIC)
      ("outputAlembic", po::value<std::string>(&exportAlembicFile)->default_value(exportAlembicFile),
          "Filename for the SfM_Data export file (where camera poses will be stored). "
          "Default : trackedcameras.abc.")
#endif
      ("outputBinary", po::value<std::string>(&exportBinaryFile)->default_value(exportBinaryFile),
          "Filename for the localization results (raw data) as .bin")

      ;
  
  allParams.add(inputParams).add(outputParams).add(commonParams).add(voctreeParams).add(bundleParams);

  po::variables_map vm;

  try
  {
    po::store(po::parse_command_line(argc, argv, allParams), vm);

    if(vm.count("help") || (argc == 1))
    {
      OPENMVG_COUT(allParams);
      return EXIT_SUCCESS;
    }

    po::notify(vm);
  }
  catch(boost::program_options::required_option& e)
  {
    OPENMVG_CERR("ERROR: " << e.what() << std::endl);
    OPENMVG_COUT("Usage:\n\n" << allParams);
    return EXIT_FAILURE;
  }
  catch(boost::program_options::error& e)
  {
    OPENMVG_CERR("ERROR: " << e.what() << std::endl);
    OPENMVG_COUT("Usage:\n\n" << allParams);
    return EXIT_FAILURE;
  }
  
  if(!checkRobustEstimator(matchingEstimator, matchingErrorMax) || 
     !checkRobustEstimator(resectionEstimator, resectionErrorMax))
  {
    return EXIT_FAILURE;
  }

  // Init descTypes from command-line string
  matchDescTypes = features::EImageDescriberType_stringToEnums(matchDescTypeNames);

  // decide the localizer to use based on the type of feature
#if OPENMVG_IS_DEFINED(OPENMVG_HAVE_CCTAG)
  useVoctreeLocalizer = !(matchDescTypes.size() == 1 &&
                        ((matchDescTypes.front() == features::EImageDescriberType::CCTAG3) ||
                        (matchDescTypes.front() == features::EImageDescriberType::CCTAG4)));
#endif
  
  // just for debugging purpose, print out all the parameters
  {
    // the bundle adjustment can be run for now only if the refine intrinsics option is not set
    globalBundle = (globalBundle && !refineIntrinsics);
    OPENMVG_COUT("Program called with the following parameters:");
    OPENMVG_COUT("\tsfmdata: " << sfmFilePath);
    OPENMVG_COUT("\tmatching descriptor types: " << matchDescTypeNames);
    OPENMVG_COUT("\tpreset: " << featurePreset);
    OPENMVG_COUT("\tresectionEstimator: " << resectionEstimator);
    OPENMVG_COUT("\tmatchingEstimator: " << matchingEstimator);
    OPENMVG_COUT("\tcalibration: " << calibFile);
    OPENMVG_COUT("\tdescriptorPath: " << descriptorsFolder);
    OPENMVG_COUT("\trefineIntrinsics: " << refineIntrinsics);
    OPENMVG_COUT("\treprojectionError: " << resectionErrorMax);
    OPENMVG_COUT("\tmediafile: " << mediaFilepath);
    if(useVoctreeLocalizer)
    {
      OPENMVG_COUT("\tvoctree: " << vocTreeFilepath);
      OPENMVG_COUT("\tweights: " << weightsFilepath);
      OPENMVG_COUT("\tnbImageMatch: " << numResults);
      OPENMVG_COUT("\tmaxResults: " << maxResults);
      OPENMVG_COUT("\tcommon views: " << numCommonViews);
      OPENMVG_COUT("\talgorithm: " << algostring);
      OPENMVG_COUT("\tmatchingError: " << matchingErrorMax);
      OPENMVG_COUT("\tnbFrameBufferMatching: " << nbFrameBufferMatching);
      OPENMVG_COUT("\trobustMatching: " << robustMatching);
    }
#if OPENMVG_IS_DEFINED(OPENMVG_HAVE_CCTAG) 
    else
    {
      OPENMVG_COUT("\tnNearestKeyFrames: " << nNearestKeyFrames);
    }
#endif 
    OPENMVG_COUT("\tminPointVisibility: " << minPointVisibility);
    OPENMVG_COUT("\tglobalBundle: " << globalBundle);
    OPENMVG_COUT("\tnoDistortion: " << noDistortion);
    OPENMVG_COUT("\tnoBArefineIntrinsics: " << noBArefineIntrinsics);
#if OPENMVG_IS_DEFINED(OPENMVG_HAVE_ALEMBIC)
    OPENMVG_COUT("\toutputAlembic: " << exportAlembicFile);
#endif
    OPENMVG_COUT("\toutputBinary: " << exportBinaryFile);
    OPENMVG_COUT("\tvisualDebug: " << visualDebug);
  }

  // if the provided directory for visual debugging does not exist create it
  // recursively
  if((!visualDebug.empty()) && (!bfs::exists(visualDebug)))
  {
    bfs::create_directories(visualDebug);
  }
 
  // this contains the full path and the root name of the file without the extension
  const bool wantsBinaryOutput = exportBinaryFile.empty();
#if OPENMVG_IS_DEFINED(OPENMVG_HAVE_ALEMBIC)
  std::string basenameAlembic = (bfs::path(exportBinaryFile).parent_path() / bfs::path(exportBinaryFile).stem()).string();
#endif
  std::string basenameBinary;
  if(wantsBinaryOutput)
  {
    basenameBinary = (bfs::path(exportBinaryFile).parent_path() / bfs::path(exportBinaryFile).stem()).string();
  }

  
  //***********************************************************************
  // Localizer initialization
  //***********************************************************************
  
  std::unique_ptr<localization::LocalizerParameters> param;
  
  std::unique_ptr<localization::ILocalizer> localizer;
  
  // initialize the localizer according to the chosen type of describer

#if OPENMVG_IS_DEFINED(OPENMVG_HAVE_CCTAG)
  if(!useVoctreeLocalizer)
  {
    localization::CCTagLocalizer* tmpLoc = new localization::CCTagLocalizer(sfmFilePath, descriptorsFolder);
    localizer.reset(tmpLoc);

    localization::CCTagLocalizer::Parameters* tmpParam = new localization::CCTagLocalizer::Parameters();
    param.reset(tmpParam);
    tmpParam->_nNearestKeyFrames = nNearestKeyFrames;
  }
  else
#endif
  {
    localization::VoctreeLocalizer* tmpLoc = new localization::VoctreeLocalizer(sfmFilePath,
                                                   descriptorsFolder,
                                                   vocTreeFilepath,
                                                   weightsFilepath,
                                                   matchDescTypes);

    localizer.reset(tmpLoc);
    
    localization::VoctreeLocalizer::Parameters *tmpParam = new localization::VoctreeLocalizer::Parameters();
    param.reset(tmpParam);
    tmpParam->_algorithm = localization::VoctreeLocalizer::initFromString(algostring);;
    tmpParam->_numResults = numResults;
    tmpParam->_maxResults = maxResults;
    tmpParam->_numCommonViews = numCommonViews;
    tmpParam->_ccTagUseCuda = false;
    tmpParam->_matchingError = matchingErrorMax;
    tmpParam->_nbFrameBufferMatching = nbFrameBufferMatching;
    tmpParam->_useRobustMatching = robustMatching;
  }
  
  assert(localizer);
  assert(param);
  
  // set other common parameters
  param->_featurePreset = featurePreset;
  param->_refineIntrinsics = refineIntrinsics;
  param->_visualDebug = visualDebug;
  param->_errorMax = resectionErrorMax;
  param->_resectionEstimator = resectionEstimator;
  param->_matchingEstimator = matchingEstimator;
  
  
  if(!localizer->isInit())
  {
    OPENMVG_CERR("ERROR while initializing the localizer!");
    return EXIT_FAILURE;
  }
  
  // create the feedProvider
  dataio::FeedProvider feed(mediaFilepath, calibFile);
  if(!feed.isInit())
  {
    OPENMVG_CERR("ERROR while initializing the FeedProvider!");
    return EXIT_FAILURE;
  }
  
#if OPENMVG_IS_DEFINED(OPENMVG_HAVE_ALEMBIC)
  // init alembic exporter
  sfm::AlembicExporter exporter( exportAlembicFile );
  exporter.initAnimatedCamera("camera");
#endif
  
  image::Image<unsigned char> imageGrey;
  cameras::Pinhole_Intrinsic_Radial_K3 queryIntrinsics;
  bool hasIntrinsics = false;
  
  std::size_t frameCounter = 0;
  std::size_t goodFrameCounter = 0;
  std::vector<std::string> goodFrameList;
  std::string currentImgName;
  
  //***********************************************************************
  // Main loop
  //***********************************************************************
  
  // Define an accumulator set for computing the mean and the
  // standard deviation of the time taken for localization
  bacc::accumulator_set<double, bacc::stats<bacc::tag::mean, bacc::tag::min, bacc::tag::max, bacc::tag::sum > > stats;
  
  std::vector<localization::LocalizationResult> vec_localizationResults;
  
  while(feed.readImage(imageGrey, queryIntrinsics, currentImgName, hasIntrinsics))
  {
    OPENMVG_COUT("******************************");
    OPENMVG_COUT("FRAME " << myToString(frameCounter,4));
    OPENMVG_COUT("******************************");
    localization::LocalizationResult localizationResult;
    auto detect_start = std::chrono::steady_clock::now();
    localizer->localize(imageGrey, 
                       param.get(),
                       hasIntrinsics /*useInputIntrinsics*/,
                       queryIntrinsics,
                       localizationResult,
                       currentImgName);
    auto detect_end = std::chrono::steady_clock::now();
    auto detect_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(detect_end - detect_start);
    OPENMVG_COUT("\nLocalization took  " << detect_elapsed.count() << " [ms]");
    stats(detect_elapsed.count());
    
    vec_localizationResults.emplace_back(localizationResult);

    // save data
    if(localizationResult.isValid())
    {
#if OPENMVG_IS_DEFINED(OPENMVG_HAVE_ALEMBIC)
      exporter.addCameraKeyframe(localizationResult.getPose(), &queryIntrinsics, currentImgName, frameCounter, frameCounter);
#endif
      
      goodFrameCounter++;
      goodFrameList.push_back(currentImgName + " : " + std::to_string(localizationResult.getIndMatch3D2D().size()) );
    }
    else
    {
      OPENMVG_CERR("Unable to localize frame " << frameCounter);
#if OPENMVG_IS_DEFINED(OPENMVG_HAVE_ALEMBIC)
      exporter.jumpKeyframe(currentImgName);
#endif
    }
    ++frameCounter;
    feed.goToNextFrame();
  }

  if(wantsBinaryOutput)
  {
    localization::save(vec_localizationResults, basenameBinary + ".bin");
  }

  //***********************************************************************
  // Global bundle
  //***********************************************************************
  
  if(globalBundle)
  {
    OPENMVG_COUT("\n\n\n***********************************************");
    OPENMVG_COUT("Bundle Adjustment - Refining the whole sequence");
    OPENMVG_COUT("***********************************************\n\n");
    // run a bundle adjustment
    const bool b_allTheSame = true;
    const bool b_refineStructure = false;
    const bool b_refinePose = true;
    const bool BAresult = localization::refineSequence(vec_localizationResults,
                                                       b_allTheSame, 
                                                       !noBArefineIntrinsics, 
                                                       noDistortion, 
                                                       b_refinePose,
                                                       b_refineStructure,
                                                       basenameBinary + ".sfmdata.BUNDLE",
                                                       minPointVisibility);
    if(!BAresult)
    {
      OPENMVG_CERR("Bundle Adjustment failed!");
    }
    else
    {
#if OPENMVG_IS_DEFINED(OPENMVG_HAVE_ALEMBIC)
      // now copy back in a new abc with the same name file and BUNDLE appended at the end
      sfm::AlembicExporter exporterBA( basenameAlembic +".BUNDLE.abc" );
      exporterBA.initAnimatedCamera("camera");
      std::size_t idx = 0;
      for(const localization::LocalizationResult &res : vec_localizationResults)
      {
        if(res.isValid())
        {
          assert(idx < vec_localizationResults.size());
          exporterBA.addCameraKeyframe(res.getPose(), &res.getIntrinsics(), currentImgName, idx, idx);
        }
        else
        {
          exporterBA.jumpKeyframe(currentImgName);
        }
        idx++;
      }

#endif
      if(wantsBinaryOutput)
      {
        localization::save(vec_localizationResults, basenameBinary +".BUNDLE.bin");
      }

    }
  }
  
  // print out some time stats
  OPENMVG_COUT("\n\n******************************");
  OPENMVG_COUT("Localized " << goodFrameCounter << "/" << frameCounter << " images");
  OPENMVG_COUT("Images localized with the number of 2D/3D matches during localization :");
  for(std::size_t i = 0; i < goodFrameList.size(); ++i)
    OPENMVG_COUT(goodFrameList[i]);
  OPENMVG_COUT("Processing took " << bacc::sum(stats)/1000 << " [s] overall");
  OPENMVG_COUT("Mean time for localization:   " << bacc::mean(stats) << " [ms]");
  OPENMVG_COUT("Max time for localization:   " << bacc::max(stats) << " [ms]");
  OPENMVG_COUT("Min time for localization:   " << bacc::min(stats) << " [ms]");
}
