#pragma once

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include "utils.h"

#include <GL/glx.h>
#include <GL/glxproto.h>

#include <GL/gl.h> // Place after glx!

#include "NvIFROpenGL.h"
#include "NvIFR_API.h"

using namespace std;

struct CaptureThreadData
{
  Display *m_pDisplay;
  int m_iScreen;
  Window m_window;
  int m_iFps;
  char m_strOutName[256];
};

int XErrorFunc(Display *dsp, XErrorEvent *error);
void GetWindowWithGivenName(Display *pDisplay, Window root,
                            vector<Window> *pWindowList,
                            const char *windowName = NULL);
void PrintWindowName(Display *pDisplay, Window window);
unsigned int captureThreadSync(void *userData);

class XCapture
{
public:
  XCapture();
  ~XCapture();

public:
  //! the NvIFR API function list
  static NvIFRAPI nvIFR;

  static void NvIFROGLInitialize();

  bool InitializeGLXContext(Display *xDisplay, int screen, Window window,
                            bool hasAlpha);
  bool InitializeGL(Window window);
  bool NvIFROGLCreateEncSession(
      const char *outPath, int fps = 30, int bitrate = 0,
      NV_IFROGL_HW_ENC_TYPE codecType = NV_IFROGL_HW_ENC_H264);

  bool CaptureSync();
  bool CaptureAsync();
  bool WriteDataAsync();

protected:
  GLuint CompileASMShader(GLenum program_type, const char *code);

protected:
  FILE *m_fpOut;

  int m_iWidth;
  int m_iHeight;

  GLXContext m_glxCtx;

  Display *m_pDisplay;

  //! the NvIFR session handle
  NV_IFROGL_SESSION_HANDLE m_hSession;
  //! the NvIFR transfer object handle
  NV_IFROGL_TRANSFEROBJECT_HANDLE m_hTransferObject;
};