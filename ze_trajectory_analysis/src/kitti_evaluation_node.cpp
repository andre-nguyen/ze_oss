// Copyright (c) 2015-2016, ETH Zurich, Wyss Zurich, Zurich Eye
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the ETH Zurich, Wyss Zurich, Zurich Eye nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL ETH Zurich, Wyss Zurich, Zurich Eye BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <string>
#include <iostream>
#include <glog/logging.h>
#include <gflags/gflags.h>

#include <ze/common/file_utils.hpp>
#include <ze/common/csv_trajectory.hpp>
#include <ze/trajectory_analysis/kitti_evaluation.hpp>

DEFINE_string(data_dir, ".", "Path to data");
DEFINE_string(filename_es, "traj_es.csv", "Filename of estimated trajectory.");
DEFINE_string(filename_gt, "traj_gt.csv", "Filename of groundtruth trajectory.");
DEFINE_string(filename_result_prefix, "traj_relative_errors", "Filename prefix of result.");
DEFINE_string(format_es, "pose", "Format of the estimate {pose, euroc, swe}.");
DEFINE_string(format_gt, "pose", "Format of the groundtruth {pose, euroc, swe}.");
DEFINE_double(offset_sec, 0.0, "time offset added to the timestamps of the estimate");
DEFINE_double(max_difference_sec, 0.02, "maximally allowed time difference for matching entries");
DEFINE_double(segment_length, 50, "Segment length of relative error evaluation. [meters]");
DEFINE_double(skip_frames, 10, "Number of frames to skip between evaluation.");
DEFINE_bool(least_squares_align, false, "Use least squares to align 20% of the segment length.");
DEFINE_bool(least_squares_align_translation_only, false, "Ignore the orientation for the LSQ alignment.");
DEFINE_double(least_squares_align_range, 0.2, "Portion of the segment that should be least squares aligned.");

ze::PoseSeries::Ptr loadData(const std::string& format,
                             const std::string& datapath)
{
  if(format == "swe")
  {
    VLOG(1) << "Loading 'swe' formatted trajectory from: " << datapath;
    ze::PoseSeries::Ptr series = std::make_shared<ze::SWEResultSeries>();
    series->load(datapath);
    return series;
  }
  else if(format == "euroc")
  {
    VLOG(1) << "Loading 'euroc' formatted trajectory from: " << datapath;
    ze::PoseSeries::Ptr series = std::make_shared<ze::EurocResultSeries>();
    series->load(datapath);
    return series;
  }
  else if(format == "pose")
  {
    VLOG(1) << "Loading 'pose' formatted trajectory from: " << datapath;
    ze::PoseSeries::Ptr series = std::make_shared<ze::PoseSeries>();
    series->load(datapath);
    return series;
  }
  LOG(FATAL) << "Format " << format << " is not supported.";
  return nullptr;
}


int main(int argc, char** argv)
{
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InstallFailureSignalHandler();
  FLAGS_alsologtostderr = true;
  FLAGS_colorlogtostderr = true;

  // Load groundtruth.
  VLOG(1) << "Load groundtruth: " << FLAGS_filename_gt;
  ze::PoseSeries::Ptr gt_data = loadData(
        FLAGS_format_gt, ze::joinPath(FLAGS_data_dir, FLAGS_filename_gt));

  // Load estimate data.
  VLOG(1) << "Load estimate: " << FLAGS_filename_es;
  ze::PoseSeries::Ptr es_data =
      loadData(FLAGS_format_es, ze::joinPath(FLAGS_data_dir, FLAGS_filename_es));

  ze::StampedTransformationVector es_stamped_poses =
      es_data->getStampedTransformationVector();

  // Now loop through all estimate stamps and find closest groundtruth-stamp.
  ze::TransformationVector es_poses, gt_poses;
  es_poses.reserve(es_stamped_poses.size());
  gt_poses.reserve(es_stamped_poses.size());
  {
    int64_t offset_nsec = ze::secToNanosec(FLAGS_offset_sec);
    int64_t max_diff_nsec = ze::secToNanosec(FLAGS_max_difference_sec);
    int n_skipped = 0;
    VLOG(1) << "Associated timestamps of " << es_stamped_poses.size() << " poses...";
    for(const auto& it : es_stamped_poses)
    {
      bool success;
      int64_t gt_stamp;
      ze::Vector7 gt_pose_data;
      std::tie(gt_stamp, gt_pose_data, success) =
          gt_data->getBuffer().getNearestValue(it.first + offset_nsec);
      CHECK(success);
      if(std::abs(gt_stamp - it.first + offset_nsec) > max_diff_nsec)
      {
        ++n_skipped;
        continue;
      }

      es_poses.push_back(it.second);
      gt_poses.push_back(ze::PoseSeries::getTransformationFromVec7(gt_pose_data));
    }
    VLOG(1) << "...done. Found " << es_poses.size() << " matches.";
  }

  // Kitti evaluation
  VLOG(1) << "Computing relative errors...";
  std::vector<ze::RelativeError> errors =
      ze::calcSequenceErrors(gt_poses, es_poses, FLAGS_segment_length,
                             FLAGS_skip_frames, FLAGS_least_squares_align,
                             FLAGS_least_squares_align_range,
                             FLAGS_least_squares_align_translation_only);
  VLOG(1) << "...done";

  // Write result to file
  std::string filename_result =
      FLAGS_filename_result_prefix + "_"
      + std::to_string(static_cast<int>(FLAGS_segment_length)) + ".csv";
  VLOG(1) << "Write result to file: " << ze::joinPath(FLAGS_data_dir, filename_result);
  std::ofstream fs;
  ze::openOutputFileStream(ze::joinPath(FLAGS_data_dir, filename_result), &fs);
  fs << "# First frame index, err-tx, err-ty, err-tz, err-ax, err-ay, err-az, length, num frames, err-scale\n";
  for(const ze::RelativeError& err : errors)
  {
    fs << err.first_frame << ", "
       << err.W_t_gt_es.x() << ", "
       << err.W_t_gt_es.y() << ", "
       << err.W_t_gt_es.z() << ", "
       << err.W_R_gt_es.x() << ", "
       << err.W_R_gt_es.y() << ", "
       << err.W_R_gt_es.z() << ", "
       << err.len << ", "
       << err.num_frames << ", "
       << err.scale_error << "\n";
  }
  fs.close();
  VLOG(1) << "Finished.";

  return 0;
}
