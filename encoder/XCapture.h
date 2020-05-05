#pragma once

// Place before GL headers
#include <X11/Xmd.h>

// GL Headers
#include <GL/glew.h>
#include <GL/glx.h>
#include <GL/glxproto.h>

// Place after glew/glx!
#include <GL/gl.h>

#include <memory>

#include "NvIFROpenGL.h"
#include "NvIFR_API.h"

using namespace std;

struct CaptureThreadData
{
  Display *m_pDisplay;
  int m_iScreen = 0;
  char m_strOutName[256];
};

class XCapture
{
public:
  XCapture();
  ~XCapture();

public:
  static void NvIFROGLInitialize();
  // bool InitializeGLXContext();
  // bool InitializeGL();
  bool NvIFROGLCreateEncSession(NV_IFROGL_HW_ENC_CONFIG *config);
  bool NvIFROGLCreateRawSession();

  bool CaptureSync();

  const void *get_encoded_frame(uintptr_t &size);
  const void *get_raw_frame(uintptr_t &size);
  bool release_last_frame();
  bool release_raw_frame();
  int getWidth();
  int getHeight();

private:
  void print_nvifr_error();

protected:
  GLuint CompileASMShader(GLenum program_type, const char *code);

protected:
  // FILE *m_fpOut = nullptr;

  int m_iWidth = 0;
  int m_iHeight = 0;

  GLXContext m_glxCtx = nullptr;

  Display *m_pDisplay = nullptr;

  //! the NvIFR API function list
  static NvIFRAPI nvIFR;
  //! the NvIFR session handle
  NV_IFROGL_SESSION_HANDLE m_hSession = nullptr;
  //! the NvIFR transfer object handle
  NV_IFROGL_TRANSFEROBJECT_HANDLE m_hTransferObject = nullptr;
  NV_IFROGL_TRANSFEROBJECT_HANDLE m_hSysTransferObject = nullptr;
};

std::unique_ptr<XCapture> init_capture(void *userData,
                                       NV_IFROGL_H264_ENC_CONFIG *enc_config);

int XErrorFunc(Display *dsp, XErrorEvent *error);