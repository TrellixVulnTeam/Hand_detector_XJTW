// Copyright 2019 The MediaPipe Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// An example of sending OpenCV webcam frames into a MediaPipe graph.
// This example requires a linux computer and a GPU with EGL support drivers.

#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <chrono>

#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/framework/port/commandlineflags.h"
#include "mediapipe/framework/port/file_helpers.h"
#include "mediapipe/framework/port/opencv_highgui_inc.h"
#include "mediapipe/framework/port/opencv_imgproc_inc.h"
#include "mediapipe/framework/port/opencv_video_inc.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/gpu/gl_calculator_helper.h"
#include "mediapipe/gpu/gpu_buffer.h"
#include "mediapipe/gpu/gpu_shared_data_internal.h"

#include "mediapipe/calculators/util/landmarks_to_render_data_calculator.pb.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/formats/detection.pb.h"
#include "mediapipe/framework/formats/classification.pb.h"


constexpr char kInputStream[] = "input_video";
constexpr char kOutputStream[] = "output_video";
constexpr char kLandmarksStream[] = "hand_landmarks";
constexpr char kLandmarkPresence[] = "landmark_presence";
constexpr char kPalmPresence[] = "palm_presence";
constexpr char kPalmDetectionsStream[] = "palm_detections";
constexpr char kHandednessStream[] = "handedness";
constexpr char kWindowName[] = "MediaPipe";
constexpr double pi = 3.14159265;

// #define distance(x1,y1, x2,y2) (sqrt(pow(x1-x2, 2) + pow(y1-y2, 2)))
// #define dotProduct(x1,y1,z1, x2,y2,z2) (x1*x2 +y1*y2 + z1*z2)

typedef struct Point
{
    double x=0;
    double y=0;
    double z=0;
} Point;

#define _POINT(landmark,i) {landmark.landmark(i).x(),landmark.landmark(i).y(),landmark.landmark(i).z()}


// static inline const double _POINT()
// {
//   return sqrt(pow(p1.x-p2.x, 2) + pow(p1.y-p2.y, 2));
// }

static inline const double distance(const Point& p1, const Point& p2)
{
  return sqrt(pow(p1.x-p2.x, 2) + pow(p1.y-p2.y, 2) + pow(p1.z-p2.z, 2));
}

static inline const Point vector(const Point& p1, const Point& p2)
{
  Point v;
  v.x = p2.x - p1.x;
  v.y = p2.y - p1.y;
  v.z = p2.z - p1.z;
  return v;
}

static inline const double dotProduct(const Point& v1, const Point& v2)
{
  return v1.x*v2.x +v1.y*v2.y + v1.z*v2.z;
}

static inline const Point crossProduct(const Point& v1, const Point& v2)
{
  Point res;
  res.x = v1.y * v2.z - v1.z * v2.y;
  res.y = -(v1.x * v2.z - v1.z * v2.x);
  res.z = v1.x * v2.y - v1.y * v2.x;
  return res;
}

static inline const double magnitude(const Point& v)
{
  return sqrt(dotProduct(v, v));
}

static inline const double angle(const Point& v1, const Point& v2)
{
  return acos (dotProduct(v1, v2) / (magnitude(v1) * magnitude(v2))) * 180.0 / pi;
}

static inline const double angle(const Point& p11, const Point& p12, const Point& p21, const Point& p22)
{
  Point v1 = vector(p11, p12);
  Point v2 = vector(p21, p22);
  return angle(v1, v2);
}

DEFINE_string(
    calculator_graph_config_file, "",
    "Name of file containing text format CalculatorGraphConfig proto.");
DEFINE_string(input_video_path, "",
              "Full path of video to load. "
              "If not provided, attempt to use a webcam.");
DEFINE_string(output_video_path, "",
              "Full path of where to save result (.mp4 only). "
              "If not provided, show result in a window.");

::mediapipe::Status RunMPPGraph() {
  std::string calculator_graph_config_contents;
  MP_RETURN_IF_ERROR(mediapipe::file::GetContents(
      FLAGS_calculator_graph_config_file, &calculator_graph_config_contents));
  LOG(INFO) << "Get calculator graph config contents: "
            << calculator_graph_config_contents;
  mediapipe::CalculatorGraphConfig config =
      mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(
          calculator_graph_config_contents);

  LOG(INFO) << "Initialize the calculator graph.";
  mediapipe::CalculatorGraph graph;
  MP_RETURN_IF_ERROR(graph.Initialize(config));

  LOG(INFO) << "Initialize the GPU.";
  ASSIGN_OR_RETURN(auto gpu_resources, mediapipe::GpuResources::Create());
  MP_RETURN_IF_ERROR(graph.SetGpuResources(std::move(gpu_resources)));
  mediapipe::GlCalculatorHelper gpu_helper;
  gpu_helper.InitializeForTest(graph.GetGpuResources().get());

  LOG(INFO) << "Initialize the camera or load the video.";
  cv::VideoCapture capture;
  std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ OPENING CAPTURE DEVICE" << std::endl;
  const bool load_video = !FLAGS_input_video_path.empty();
  if (load_video) {
    capture.open(FLAGS_input_video_path);
  } else {
    capture.open(0);
  }
  std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ OPENED CAPTURE DEVICE ? " << capture.isOpened() << std::endl;
  RET_CHECK(capture.isOpened());

  cv::VideoWriter writer;
  const bool save_video = !FLAGS_output_video_path.empty();
  std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ SETTING CAPTURE DEVICE SETTINGS" << std::endl;
  if (!save_video) {
    cv::namedWindow(kWindowName, /*flags=WINDOW_AUTOSIZE*/ 1);
#if (CV_MAJOR_VERSION >= 3) && (CV_MINOR_VERSION >= 2)
    capture.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    capture.set(cv::CAP_PROP_FPS, 20);
#endif
  std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ SET CAPTURE DEVICE SETTINGS ? DONE" << std::endl;
  }

  LOG(INFO) << "Start running the calculator graph.";
  ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller presence_landmark_poller,
                   graph.AddOutputStreamPoller(kLandmarkPresence));
  ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller presence_palm_poller,
                   graph.AddOutputStreamPoller(kPalmPresence));
  ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller poller,
                   graph.AddOutputStreamPoller(kOutputStream));
  ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller poller_landmark,
            graph.AddOutputStreamPoller(kLandmarksStream));
  ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller detection_poller,
                  graph.AddOutputStreamPoller(kPalmDetectionsStream));
  ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller handedness_poller,///////////////////////////////////
                  graph.AddOutputStreamPoller(kHandednessStream));
  MP_RETURN_IF_ERROR(graph.StartRun({}));

  LOG(INFO) << "Start grabbing and processing frames.";
  bool grab_frames = true;
  int frame_count_thresh = 20;
  int frame_counter = 0;

  while (grab_frames) {
    if (frame_counter <= frame_count_thresh)
      frame_counter++;
    // Capture opencv camera or video frame.
    cv::Mat camera_frame_raw;
    capture >> camera_frame_raw;
    if (camera_frame_raw.empty()) break;  // End of video.
    cv::Mat camera_frame;
    cv::cvtColor(camera_frame_raw, camera_frame, cv::COLOR_BGR2RGBA);
    if (!load_video) {
      cv::flip(camera_frame, camera_frame, /*flipcode=HORIZONTAL*/ 1);
    }
    // Wrap Mat into an ImageFrame.
    auto input_frame = absl::make_unique<mediapipe::ImageFrame>(
        mediapipe::ImageFormat::SRGBA, camera_frame.cols, camera_frame.rows,
        mediapipe::ImageFrame::kGlDefaultAlignmentBoundary);
    cv::Mat input_frame_mat = mediapipe::formats::MatView(input_frame.get());
    camera_frame.copyTo(input_frame_mat);
    // Prepare and add graph input packet.
    size_t frame_timestamp_us =
        (double)cv::getTickCount() / (double)cv::getTickFrequency() * 1e6;
    MP_RETURN_IF_ERROR(
        gpu_helper.RunInGlContext([&input_frame, &frame_timestamp_us, &graph,
                                   &gpu_helper]() -> ::mediapipe::Status {
          // Convert ImageFrame to GpuBuffer.
          auto texture = gpu_helper.CreateSourceTexture(*input_frame.get());
          auto gpu_frame = texture.GetFrame<mediapipe::GpuBuffer>();
          glFlush();
          texture.Release();
          // Send GPU image packet into the graph.
          MP_RETURN_IF_ERROR(graph.AddPacketToInputStream(
              kInputStream, mediapipe::Adopt(gpu_frame.release())
                                .At(mediapipe::Timestamp(frame_timestamp_us))));
          return ::mediapipe::OkStatus();
        }));
    // Get the graph result packet, or stop if that fails.
    mediapipe::Packet packet;
    mediapipe::Packet landmark_packet;
    mediapipe::Packet detection_packet;
    mediapipe::Packet presence_landmark_packet;
    mediapipe::Packet presence_palm_packet;
    mediapipe::Packet handedness_packet;
    
    std::unique_ptr<mediapipe::ImageFrame> output_frame;
    if (!poller.Next(&packet)) break;
    if (!presence_palm_poller.Next(&presence_palm_packet)) break;
    auto is_palm_present = presence_palm_packet.Get<bool>();
    // if(is_palm_present) {
    // if (detection_poller.Next(&detection_packet)) {
    //   auto& output_detections = detection_packet.Get<std::vector<::mediapipe::Detection>>();
    //   int i = 0;
    //   for (const ::mediapipe::Detection& det : output_detections) {
    //     i++;
    //     std::cout << "score = " << det.score(0) << ", i=" << i << std::endl;
    //   }
    // }
    // } 
    if (!presence_landmark_poller.Next(&presence_landmark_packet)) break;
    auto is_landmark_present = presence_landmark_packet.Get<bool>();
    bool has_landmark = false;
    bool has_handedness = false;
    if(is_landmark_present) {
      has_landmark = poller_landmark.Next(&landmark_packet);
      // has_handedness = handedness_poller.Next(&handedness_packet);
    }

    if (!is_palm_present) {
        // if(has_handedness) {
        //   auto &output_handedness = handedness_packet.Get<std::vector<::mediapipe::ClassificationList>>();
        //   for (const ::mediapipe::ClassificationList& classification : output_handedness) {
        //     /* 
        //     TODO:
        //       1. Make double score_thresh = 0.99989 variable.
        //       2. Save the boolean values corresponding to the score condition (handedness score < score_thresh) as a boolean array with the size of hands detected in the current frame.
        //       3. use the boolean value to decide whether to show each hand frame / finger counting or not.
        //       4. Check the code with different number of hand detections (>1).
        //     THE END!
        //     */ 
        //     auto &temp = classification.classification(0).score();
        //     std::cout << temp.label() << ", " << temp.score() << std::endl;
        //   }
        // }
        if (has_landmark && frame_counter > frame_count_thresh) {
          auto& output_landmarks = landmark_packet.Get<std::vector<::mediapipe::NormalizedLandmarkList>>();
          int count = 0;
          double thresh_lbm = 40, thresh_lmp = 120, thresh_thumb = 55;
          double thresh_joint = 160, thresh_thumb_dist = 0.0435, fingers_dist_thresh = 0.1;
          bool res[5] = { false, false, false, false, false };
          // bool skip_ear_detections = true;

          for (const ::mediapipe::NormalizedLandmarkList& landmark : output_landmarks) {

            // // double all_distances[21*21];
            // for (int i = 0; i < 21 && skip_ear_detections; i++) {
            //   Point p = _POINT(landmark, i);
            //   for (int j = 0; j < 21 && skip_ear_detections; j++) {
            //     double d = distance(_POINT(landmark, j), p);
            //     if (d > fingers_dist_thresh && d > 0)
            //       skip_ear_detections = false;
            //     else
            //       std::cout << "d = " << d << std::endl;
            //   }
            // }
            // if (skip_ear_detections) break;

            

            Point base = _POINT(landmark, 0);

            { // thumb edge case
              Point point = _POINT(landmark, 4);
              Point finger_limit = _POINT(landmark, 5);
              Point pinky_limit = _POINT(landmark, 17);
              Point middle = _POINT(landmark, 2);

              double thumb = angle(base, point, base, pinky_limit);

              if (point.z < 0) {
                double joint_angle = angle(middle, base, middle, point);
                // std::cout << "joint = " << joint_angle << std::endl;
                res[0] = thumb > thresh_thumb && joint_angle > thresh_joint;
              } else {
                double d = distance(finger_limit, middle);
                res[0] = thumb > thresh_thumb && d > thresh_thumb_dist;
              }
                
              if(res[0]) count++;
            }

            for (int i=1; i<5; i++) {
              int b = i * 4;
              Point limit = _POINT(landmark, b+1);
              Point middle = _POINT(landmark, b+2);
              Point point = _POINT(landmark, b+4);

              double lbm = angle(base, limit, base, middle);
              double lmp = angle(middle, limit, middle, point);

              res[i] = lbm < thresh_lbm && lmp > thresh_lmp;
              if(res[i]) count++;
            }
          }

          std::cout << "Finger Counting: " << count << std::endl;
          for(int j = 0; j < 5; j++) {
            std::cout << res[j] << ", ";
          }
          std::cout << std::endl;
        }
    } else {
      frame_counter = 0;
    }
  
    // Convert GpuBuffer to ImageFrame.
    MP_RETURN_IF_ERROR(gpu_helper.RunInGlContext(
        [&packet, &output_frame, &gpu_helper]() -> ::mediapipe::Status {
          auto& gpu_frame = packet.Get<mediapipe::GpuBuffer>();

          auto texture = gpu_helper.CreateSourceTexture(gpu_frame);
          output_frame = absl::make_unique<mediapipe::ImageFrame>(
              mediapipe::ImageFormatForGpuBufferFormat(gpu_frame.format()),
              gpu_frame.width(), gpu_frame.height(),
              mediapipe::ImageFrame::kGlDefaultAlignmentBoundary);
          gpu_helper.BindFramebuffer(texture);
          const auto info =
              mediapipe::GlTextureInfoForGpuBufferFormat(gpu_frame.format(), 0);
          glReadPixels(0, 0, texture.width(), texture.height(), info.gl_format,
                        info.gl_type, output_frame->MutablePixelData());
          glFlush();
          texture.Release();
          return ::mediapipe::OkStatus();
        }));
    
    // Convert back to opencv for display or saving.
    cv::Mat output_frame_mat = !is_palm_present && frame_counter > frame_count_thresh ? mediapipe::formats::MatView(output_frame.get()) : input_frame_mat;
    cv::cvtColor(output_frame_mat, output_frame_mat, cv::COLOR_RGB2BGR);
    if (save_video) {
      if (!writer.isOpened()) {
        LOG(INFO) << "Prepare video writer.";
        writer.open(FLAGS_output_video_path,
                    mediapipe::fourcc('a', 'v', 'c', '1'),  // .mp4
                    capture.get(cv::CAP_PROP_FPS), output_frame_mat.size());
        RET_CHECK(writer.isOpened());
      }
      writer.write(output_frame_mat);
    } else {
      cv::imshow(kWindowName, output_frame_mat);
      
      // Press any key to exit.
      const int pressed_key = cv::waitKey(5);
      if (pressed_key >= 0 && pressed_key != 255) grab_frames = false;
    }
  }

  LOG(INFO) << "Shutting down.";
  if (writer.isOpened()) writer.release();
  MP_RETURN_IF_ERROR(graph.CloseInputStream(kInputStream));
  return graph.WaitUntilDone();
}

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  ::mediapipe::Status run_status = RunMPPGraph();
  if (!run_status.ok()) {
    LOG(ERROR) << "Failed to run the graph: " << run_status.message();
    return EXIT_FAILURE;
  } else {
    LOG(INFO) << "Success!";
  }
  return EXIT_SUCCESS;
}
