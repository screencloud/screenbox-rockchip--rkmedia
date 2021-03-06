// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <thread>

#include "buffer.h"
#include "encoder.h"
#include "flow.h"
#include "image.h"
#include "key_string.h"
#include "media_config.h"
#include "media_type.h"
#include "message.h"
#include "stream.h"
#include "utils.h"

static bool quit = false;
static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

static void print_usage() {
  printf("On PC:\n");
  printf("Streame 1: ffplay -rtsp_transport tcp -stimeout 2000000 "
         "rtsp://admin:123456@192.168.xxx.xxx/main_stream1\n");
  printf("Streame 2: ffplay -rtsp_transport tcp -stimeout 2000000 "
         "rtsp://admin:123456@192.168.xxx.xxx/main_stream2\n");
}

std::shared_ptr<easymedia::Flow>
create_live555_rtsp_server_flow(std::string channel_name,
                                std::string media_type) {
  std::shared_ptr<easymedia::Flow> rtsp_flow;

  std::string flow_name;
  std::string flow_param;

  flow_name = "live555_rtsp_server";
  flow_param = "";
  PARAM_STRING_APPEND(flow_param, KEY_INPUTDATATYPE, media_type);
  PARAM_STRING_APPEND(flow_param, KEY_CHANNEL_NAME, channel_name);
  PARAM_STRING_APPEND_TO(flow_param, KEY_PORT_NUM, 554);

  printf("\nRtspFlow:\n%s\n", flow_param.c_str());
  rtsp_flow = easymedia::REFLECTOR(Flow)::Create<easymedia::Flow>(
      flow_name.c_str(), flow_param.c_str());
  if (!rtsp_flow) {
    fprintf(stderr, "Create flow %s failed\n", flow_name.c_str());
    // exit(EXIT_FAILURE);
  }
  return rtsp_flow;
}
std::shared_ptr<easymedia::Flow>
create_video_enc_flow(std::string pixel_format, std::string video_enc_type,
                      int video_width, int video_height, int video_fps) {
  std::shared_ptr<easymedia::Flow> video_encoder_flow;

  std::string flow_name;
  std::string flow_param;
  std::string enc_param;

  flow_name = "video_enc";
  flow_param = "";
  PARAM_STRING_APPEND(flow_param, KEY_NAME, "rkmpp");
  PARAM_STRING_APPEND(flow_param, KEY_INPUTDATATYPE, pixel_format);
  PARAM_STRING_APPEND(flow_param, KEY_OUTPUTDATATYPE, video_enc_type);

  MediaConfig enc_config;
  memset(&enc_config, 0, sizeof(enc_config));
  VideoConfig &vid_cfg = enc_config.vid_cfg;
  ImageConfig &img_cfg = vid_cfg.image_cfg;
  img_cfg.image_info.pix_fmt = StringToPixFmt(pixel_format.c_str());
  img_cfg.image_info.width = video_width;
  img_cfg.image_info.height = video_height;
  img_cfg.image_info.vir_width = UPALIGNTO16(video_width);
  img_cfg.image_info.vir_height = UPALIGNTO16(video_height);
  if ((video_enc_type == VIDEO_H264) || (video_enc_type == VIDEO_H265)) {
    img_cfg.qp_init = 24;
    vid_cfg.qp_step = 4;
    vid_cfg.qp_min = 12;
    vid_cfg.qp_max = 48;
    vid_cfg.bit_rate = video_width * video_height * 7;
    if (vid_cfg.bit_rate > 1000000) {
      vid_cfg.bit_rate /= 1000000;
      vid_cfg.bit_rate *= 1000000;
    }
    vid_cfg.frame_rate = video_fps;
    vid_cfg.level = 52;
    vid_cfg.gop_size = video_fps;
    vid_cfg.profile = 100;
    // vid_cfg.rc_quality = "aq_only"; vid_cfg.rc_mode = "vbr";
    vid_cfg.rc_quality = KEY_BEST;
    vid_cfg.rc_mode = KEY_CBR;
  } else if (video_enc_type == IMAGE_JPEG)
    img_cfg.qp_init = 10;

  enc_param = "";
  enc_param.append(easymedia::to_param_string(enc_config, video_enc_type));
  flow_param = easymedia::JoinFlowParam(flow_param, 1, enc_param);
  printf("\n#VideoEncoder flow param:\n%s\n", flow_param.c_str());
  video_encoder_flow = easymedia::REFLECTOR(Flow)::Create<easymedia::Flow>(
      flow_name.c_str(), flow_param.c_str());
  if (!video_encoder_flow) {
    fprintf(stderr, "Create flow %s failed\n", flow_name.c_str());
    // exit(EXIT_FAILURE);
  }
  return video_encoder_flow;
}
std::shared_ptr<easymedia::Flow>
create_video_read_flow(std::string input_path, std::string pixel_format,
                       int video_width, int video_height) {
  std::string flow_name;
  std::string flow_param;
  std::string stream_param;
  std::shared_ptr<easymedia::Flow> video_read_flow;

  // Reading yuv from camera
  flow_name = "source_stream";
  flow_param = "";
  PARAM_STRING_APPEND(flow_param, KEY_NAME, "v4l2_capture_stream");
  // PARAM_STRING_APPEND_TO(flow_param, KEY_FPS, video_fps);
  PARAM_STRING_APPEND(flow_param, KEK_THREAD_SYNC_MODEL, KEY_SYNC);
  PARAM_STRING_APPEND(flow_param, KEK_INPUT_MODEL, KEY_DROPFRONT);
  PARAM_STRING_APPEND_TO(flow_param, KEY_INPUT_CACHE_NUM, 5);
  stream_param = "";
  PARAM_STRING_APPEND_TO(stream_param, KEY_USE_LIBV4L2, 1);
  PARAM_STRING_APPEND(stream_param, KEY_DEVICE, input_path);
  // PARAM_STRING_APPEND(param, KEY_SUB_DEVICE, sub_input_path);
  PARAM_STRING_APPEND(stream_param, KEY_V4L2_CAP_TYPE,
                      KEY_V4L2_C_TYPE(VIDEO_CAPTURE));
  PARAM_STRING_APPEND(stream_param, KEY_V4L2_MEM_TYPE,
                      KEY_V4L2_M_TYPE(MEMORY_DMABUF));
  PARAM_STRING_APPEND_TO(stream_param, KEY_FRAMES,
                         4); // if not set, default is 2
  PARAM_STRING_APPEND(stream_param, KEY_OUTPUTDATATYPE, pixel_format);
  PARAM_STRING_APPEND_TO(stream_param, KEY_BUFFER_WIDTH, video_width);
  PARAM_STRING_APPEND_TO(stream_param, KEY_BUFFER_HEIGHT, video_height);

  flow_param = easymedia::JoinFlowParam(flow_param, 1, stream_param);
  printf("\n#VideoCapture flow param:\n%s\n", flow_param.c_str());
  video_read_flow = easymedia::REFLECTOR(Flow)::Create<easymedia::Flow>(
      flow_name.c_str(), flow_param.c_str());
  if (!video_read_flow) {
    fprintf(stderr, "Create flow %s failed\n", flow_name.c_str());
    // exit(EXIT_FAILURE);
  }
  return video_read_flow;
}

int main() {
  print_usage();
  // test 1280*720 h264
  std::shared_ptr<easymedia::Flow> video_read_flow =
      create_video_read_flow("/dev/video0", IMAGE_NV12, 1280, 720);
  std::shared_ptr<easymedia::Flow> video_enc_flow =
      create_video_enc_flow(IMAGE_NV12, VIDEO_H264, 1280, 720, 30);
  std::shared_ptr<easymedia::Flow> rtsp_flow =
      create_live555_rtsp_server_flow("main_stream1", VIDEO_H264);
  if (video_enc_flow && video_read_flow && rtsp_flow) {
    video_enc_flow->AddDownFlow(rtsp_flow, 0, 0);
    video_read_flow->AddDownFlow(video_enc_flow, 0, 0);
  }

  // test 640*480 h264
  std::shared_ptr<easymedia::Flow> video_read_flow_1 =
      create_video_read_flow("/dev/video1", IMAGE_NV12, 640, 480);
  std::shared_ptr<easymedia::Flow> video_enc_flow_1 =
      create_video_enc_flow(IMAGE_NV12, VIDEO_H264, 640, 480, 30);
  std::shared_ptr<easymedia::Flow> rtsp_flow_1 =
      create_live555_rtsp_server_flow("main_stream2", VIDEO_H264);
  if (video_enc_flow_1 && rtsp_flow_1 && video_read_flow_1) {
    video_enc_flow_1->AddDownFlow(rtsp_flow_1, 0, 0);
    video_read_flow_1->AddDownFlow(video_enc_flow_1, 0, 0);
  }

  signal(SIGINT, sigterm_handler);
  while (!quit)
    easymedia::msleep(100);

  // release test1
  if (video_read_flow) {
    if (video_enc_flow) {
      video_read_flow->RemoveDownFlow(video_enc_flow);
      if (rtsp_flow) {
        video_enc_flow->RemoveDownFlow(rtsp_flow);
        rtsp_flow.reset();
      }
      video_enc_flow.reset();
    }
    video_read_flow.reset();
  }
  // release test2
  if (video_read_flow_1) {
    if (video_enc_flow_1) {
      video_read_flow_1->RemoveDownFlow(video_enc_flow_1);
      if (rtsp_flow_1) {
        video_enc_flow_1->RemoveDownFlow(rtsp_flow_1);
        rtsp_flow_1.reset();
      }
      video_enc_flow_1.reset();
    }
    video_read_flow_1.reset();
  }
  return 0;
}
