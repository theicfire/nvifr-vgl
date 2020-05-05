#pragma once
#include <climits>
#include <memory>
#include <vector>

#include "XCapture.h"

namespace encoder
{
class Encoder
{
public:
  Encoder();
  std::vector<uint8_t> encode_frame();
  std::vector<uint8_t> get_raw_frame();
  void open_file_to_write(string filename);
  bool write_to_file(const uint8_t *data, uint32_t num_bytes);
  bool write_to_file(string filename, const uint8_t *data, uint32_t num_bytes);
  void release_frame();
  void release_raw_frame();
  void init_config();
  int get_width();
  int get_height();

private:
  bool init_encoder(CaptureThreadData &config);
  NV_IFROGL_HW_ENC_CONFIG get_init_config();
  uint32_t width, height;
  FILE *fp = nullptr;
  std::unique_ptr<XCapture> capture = nullptr;
  CaptureThreadData config;
};
} // namespace encoder

int run_test();
