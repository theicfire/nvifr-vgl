#include "encoder.h"

using namespace encoder;

unsigned int NV_H264_PROFILE_MAIN_GUID = 100;

Encoder::Encoder()
{
  // if (!display)
  //   display = WindowManager::get_default_display();

  // config.m_iScreen = DefaultScreen(display);
  // config.m_pDisplay = display;
  // config.m_window = window;

  if (!init_encoder(config))
  {
    throw runtime_error("Error: Could not initialize IFR capture");
  }
}

bool Encoder::init_encoder(CaptureThreadData &config)
{
  // if (globals::debug)
  // {
  //   XSynchronize(config.m_pDisplay, true);
  // }

  // XSetErrorHandler(XErrorFunc);

  // if (config.m_window)
  // {
  printf("nvifrog initialize\n");
  XCapture::NvIFROGLInitialize();
  printf("done nvifrog initialize\n");

  // this->wm = std::make_unique<WindowManager>();
  // string title = wm->get_title(config.m_window);

  string out_file = "window.h264";
  sprintf(config.m_strOutName, "%s", out_file.c_str());

  printf("init config..\n");
  NV_IFROGL_HW_ENC_CONFIG enc_config = get_init_config();
  if (this->capture != nullptr)
  {
    // log_error("Is calling init_encoder twice ok?");
    this->capture = nullptr; // deconstruct object before making another
  }
  printf("init capture..\n");
  this->capture = init_capture(&config, &enc_config);
  printf("done init capture..\n");

  if (!capture)
    return false;
  // log_info("[{}] - Capturing: {} @ {}x{}", __func__, title, this->get_width(),
  //  this->get_height());
  //   return true;
  // }

  return true;
}

NV_IFROGL_HW_ENC_CONFIG Encoder::get_init_config()
{
  NV_IFROGL_HW_ENC_CONFIG config;
  memset(&config, 0, sizeof(config));

  /** FPS only needed when using NV_IFROGL_HW_ENC_RATE_CONTROL_VBR */
  // unsigned int fps_scaled_bitrate =
  //     this->fps == globals::fps_60 ? this->bitrate * 2 : this->bitrate;
  // config.frameRateNum = this->fps;
  // config.avgBitRate = fps_scaled_bitrate;
  // config.peakBitRate = fps_scaled_bitrate * 1.50;
  // config.VBVBufferSize =
  //     (config.avgBitRate * config.frameRateDen / config.frameRateNum) * 5;
  // config.VBVInitialDelay = config.VBVBufferSize;

  config.profile = NV_H264_PROFILE_MAIN_GUID;
  config.frameRateDen = 1;
  config.GOPLength = UINT_MAX;
  // config.enableIntraRefresh = NV_IFROGL_BOOL_TRUE;

  /** CONSTQP instead of NV_IFROGL_HW_ENC_RATE_CONTROL_VBR because
   * EncoderSettledMetric can settle faster */
  config.rateControl = NV_IFROGL_HW_ENC_RATE_CONTROL_CONSTQP;
  config.codecType = NV_IFROGL_HW_ENC_H264;
  config.enableIntraRefresh = NV_IFROGL_BOOL_FALSE;
  config.quantizationParam = 25; //globals::host::encoder_qp;
  config.slicingMode = NV_IFROGL_HW_ENC_SLICING_MODE_DISABLED;
  config.preset = NV_IFROGL_HW_ENC_PRESET_LOW_LATENCY_HP;
  config.stereoFormat = NV_IFROGL_HW_ENC_STEREO_NONE;
  config.hwEncInputFormat = NV_IFROGL_HW_ENC_INPUT_FORMAT_YUV420;

  return config;
}

std::vector<uint8_t> Encoder::encode_frame()
{
  // if (this->wm->exists(config.m_window))
  // {
  uintptr_t size;
  const void *frame = this->capture->get_encoded_frame(size);
  if (frame == 0)
  {
    // log_error("encoded frame pointer is 0");
    return vector<uint8_t>();
  }
  else if (size == 0)
  {
    // log_error("Size of encoded frame is 0");
  }
  else
  {
    std::vector<uint8_t> ret((const uint8_t *)frame,
                             (const uint8_t *)frame + size); // TODO slow copy
    return ret;
  }
  // }

  return vector<uint8_t>();
}

std::vector<uint8_t> Encoder::get_raw_frame()
{
  // if (this->wm->exists(config.m_window))
  // {
  uintptr_t size;
  const void *frame = this->capture->get_raw_frame(size);
  if (frame == 0)
  {
    // log_error("Raw frame pointer is 0");
    return vector<uint8_t>();
  }
  else if (size == 0)
  {
    // log_error("Size of raw frame is 0");
    return vector<uint8_t>();
  }
  else
  {
    std::vector<uint8_t> ret((const uint8_t *)frame,
                             (const uint8_t *)frame + size); // TODO slow copy
    return ret;
  }
  // }

  return vector<uint8_t>();
}

void Encoder::open_file_to_write(string filename)
{
  this->fp = fopen(filename.c_str(), "wb");

  if (!this->fp)
  {
    // log_error("Couldn't open {} to write to", filename);
    return;
  }
}

int Encoder::get_width() { return this->capture->getWidth(); }

int Encoder::get_height() { return this->capture->getHeight(); }

bool Encoder::write_to_file(const uint8_t *data, uint32_t num_bytes)
{
  if (this->fp && num_bytes > 0)
  {
    // int written = fwrite(data, 1, num_bytes, this->fp);
    // log_info("wrote {} bytes to main file", written);
    return true;
  }

  return false;
}

bool Encoder::write_to_file(string filename, const uint8_t *data,
                            uint32_t num_bytes)
{
  FILE *my_fp = fopen(filename.c_str(), "wb");
  if (my_fp && num_bytes > 0)
  {
    // int written = fwrite(data, 1, num_bytes, my_fp);
    // log_info("wrote {} bytes to {}", written, filename);
    fclose(my_fp);
    return true;
  }
  return false;
}

void Encoder::release_frame() { this->capture->release_last_frame(); }

void Encoder::release_raw_frame() { this->capture->release_raw_frame(); }

// int run_test()
// {
//   Timer t;

//   auto wm = std::make_unique<WindowManager>();
//   globals::host::app_window.window_id =
//       wm->get_first_app_window(globals::host::window_class_name.c_str());
//   globals::host::app_window.display =
//       window::WindowManager::get_default_display();

//   Encoder *encoder = new Encoder((Display *)globals::host::app_window.display,
//                                  (Window)globals::host::app_window.window_id);

//   encoder->open_file_to_write("window.h264");

//   while (true)
//   {
//     t.reset();
//     std::vector<uint8_t> frame = encoder->encode_frame();
//     encoder->write_to_file(frame.data(), frame.size());
//     // log_info("captured in {:2f}ms", t.getElapsedMilliseconds());
//     encoder->release_frame();
//     usleep(33000);
//   }
//   return 0;
// }
