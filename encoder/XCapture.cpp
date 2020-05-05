/**
 * Based off of
 * Capture_Linux_v7.0.5/NvIFROpenGL/samples/OpenGLIFRChrome/XCapture.cpp
 *
 * (Capture_Linux_v7.0.5 downloaded from NVIDIA, in Mighty's Google Drive)
 */
#include "XCapture.h"
#include <memory>

#include <signal.h>
#include <stdio.h>

// Force close all processes, and let systemd restart the host, if we hit this
// error. See https://mighty.height.app/T-164
int glx_make_current_error_count = 0;

NvIFRAPI XCapture::nvIFR;
NV_IFROGL_TO_SYS_CONFIG to_sys_config;

XCapture::XCapture() : m_glxCtx(NULL)
{
}

void nvifrog_force_exit()
{
  printf("nvifr force exit\n");
  constexpr int NVIFROG_FAIL_EXIT_CODE = 9;
  // Using _exit instead of exit because it may help reduce NvIFR segfaults.
  // This isn't confirmed, because creating the segfaults is pretty challenging
  // to do.
  // The reason it may help is it won't call the XCapture destructor, which may
  // be creating the segfaults.
  exit(NVIFROG_FAIL_EXIT_CODE); // eeek
}

XCapture::~XCapture()
{
  // delete NvIFR objects
  if (m_hTransferObject != nullptr &&
      XCapture::nvIFR.nvIFROGLDestroyTransferObject(m_hTransferObject) !=
          NV_IFROGL_SUCCESS)
  {
    return;
  }

  if (m_hSysTransferObject != nullptr &&
      XCapture::nvIFR.nvIFROGLDestroyTransferObject(m_hSysTransferObject) !=
          NV_IFROGL_SUCCESS)
  {
    return;
  }

  if (m_hSession != nullptr &&
      XCapture::nvIFR.nvIFROGLDestroySession(m_hSession) != NV_IFROGL_SUCCESS)
  {
    return;
  }

  // if (NULL != m_glxCtx)
  // {
  //   glXDestroyContext(m_pDisplay, m_glxCtx);
  // }
}

void XCapture::NvIFROGLInitialize()
{
  if (nvIFR.initialize() == false)
  {
    // log_error("Failed to create NvIFROGL instance");
    exit(1);
  }
}

// bool XCapture::InitializeGLXContext(Display *xDisplay, int screen, bool hasAlpha)
// {
//   if (!wm->exists(window))
//   {
//     // log_error("[{}] Window {} does not exist", __func__, window);
//     return false;
//   }

//   m_pDisplay = xDisplay;

//   int visualAttribs[] = {GLX_RGBA, GLX_ALPHA_SIZE, (int)hasAlpha, None};

//   XVisualInfo *visualInfo = glXChooseVisual(m_pDisplay, screen, visualAttribs);
//   if (NULL == visualInfo)
//   {
//     // log_error("Creating visual info failed");
//     return false;
//   }

//   // Create a GLXContext compatible with window
//   m_glxCtx = glXCreateContext(m_pDisplay, visualInfo, NULL, true);
//   if (NULL == m_glxCtx)
//   {
//     // log_error("Creating glxContext failed");
//   }
//   XFree(visualInfo);

//   // bind GLXContext to the window
//   if (!glXMakeCurrent(m_pDisplay, window, m_glxCtx))
//   {
//     glXDestroyContext(m_pDisplay, m_glxCtx);
//     return false;
//   }

//   // log_info("Initialized GLX context successfully...");

//   return true;
// }

// bool XCapture::InitializeGL(Window window)
// {
//   if (!wm->exists(window))
//   {
//     // log_error("[{}] Window does not exist", string(__func__));
//     return false;
//   }

//   // log_info("[{}] window: {}", __func__, window);

//   auto genom = this->wm->get_geom(window);
//   m_iWidth = genom->width;
//   m_iHeight = genom->height;
//   // log_info("m_iWidth: {}, m_iHeight: {}", m_iWidth, m_iHeight);

//   // Init OpenGL
//   glewInit();
//   // cudaGLSetDevice(0);
//   glClearColor(0.0, 0.0, 0.0, 1.0); // 1.0 => A for color conversion matrix
//   glClear(GL_COLOR_BUFFER_BIT);
//   glViewport(0, 0, m_iWidth, m_iHeight);
//   glEnable(GL_TEXTURE_2D);
//   glEnable(GL_BLEND);
//   glDisable(GL_DEPTH_TEST);
//   glEnable(GL_CULL_FACE);
//   glMatrixMode(GL_PROJECTION);
//   glLoadIdentity();
//   glOrtho(0.0, 1.0, 0.0, 1.0, 0.0, 1.0);
//   glMatrixMode(GL_MODELVIEW);
//   glDrawBuffer(GL_BACK);

//   return true;
// }

int XCapture::getWidth() { return m_iWidth; }

int XCapture::getHeight() { return m_iHeight; }

bool XCapture::NvIFROGLCreateEncSession(NV_IFROGL_HW_ENC_CONFIG *config)
{
  // A session is required. The session is associated with the current OpenGL
  // context.
  printf("ya create sess\n");
  if (XCapture::nvIFR.nvIFROGLCreateSession(&m_hSession, NULL) !=
      NV_IFROGL_SUCCESS)
  {
    exit(1);
  }
  printf("done create sess\n");

  config->width = this->getWidth();
  config->height = this->getHeight();

  int ret;
  printf("ya create trans\n");
  if ((ret = XCapture::nvIFR.nvIFROGLCreateTransferToHwEncObject(
           m_hSession, config, &m_hTransferObject)) != NV_IFROGL_SUCCESS)
  {
    // log_error(
    // "Lame! NvIFROGLCreateEncSession Failed to create a transfer object "
    // "with code {}. "
    // "w={}, h={}",
    // ret, config->width, config->height);
    m_hTransferObject = nullptr;
    m_hSession = nullptr;
    nvifrog_force_exit();
    return false;
  }

  return true;
}

bool XCapture::NvIFROGLCreateRawSession()
{
  to_sys_config.format = NV_IFROGL_TARGET_FORMAT_YUV420P;
  int ret;
  if ((ret = XCapture::nvIFR.nvIFROGLCreateTransferToSysObject(
           m_hSession, &to_sys_config, &m_hSysTransferObject)) !=
      NV_IFROGL_SUCCESS)
  {
    // log_error(
    // "Lame! NvIFROGLCreateRawSession Failed to create a transfer object "
    // "with code {}. ",
    // ret);
    m_hSysTransferObject = nullptr;
    m_hSession = nullptr;
    nvifrog_force_exit();
    return false;
  }

  return true;
}

bool XCapture::release_raw_frame()
{
  if (XCapture::nvIFR.nvIFROGLReleaseTransferData(m_hSysTransferObject) !=
      NV_IFROGL_SUCCESS)
  {
    // log_warn("release_raw_frame: Failed to release the transferred data");
    return false;
  }
  return true;
}

bool XCapture::release_last_frame()
{
  if (XCapture::nvIFR.nvIFROGLReleaseTransferData(m_hTransferObject) !=
      NV_IFROGL_SUCCESS)
  {
    // log_warn("release_last_frame: Failed to release the transferred data");
    return false;
  }
  return true;
}

const void *XCapture::get_encoded_frame(uintptr_t &size)
{
  const void *data;
  // transfer the framebuffer
  if (XCapture::nvIFR.nvIFROGLTransferFramebufferToHwEnc(
          m_hTransferObject, NULL, 0, GL_FRONT_LEFT, GL_NONE) !=
      NV_IFROGL_SUCCESS)
  {
    // log_error("get_encoded_frame: Failed to transfer framebuffer data");
    nvifrog_force_exit();
    return 0;
  }

  // lock the transferred data
  if (XCapture::nvIFR.nvIFROGLLockTransferData(m_hTransferObject, &size,
                                               &data) != NV_IFROGL_SUCCESS)
  {
    // log_error("Failed to lock the transferred data");
    nvifrog_force_exit();
    return 0;
  }

  return data;
}

int round_to_even(int n) { return (n / 2) * 2; }

void XCapture::print_nvifr_error()
{
  char errorString[200];
  memset(errorString, 0, 200);
  unsigned int returnedLen, remainingBytes;
  nvIFR.nvIFROGLGetError(errorString, 200, &returnedLen, &remainingBytes);
  // log_error("nvifrogl error: {}", errorString);
}

const void *XCapture::get_raw_frame(uintptr_t &size)
{
  /* Raw frames are only used for detecting changes. We scale down before
   * sending to RAM to save CPU. We assume that if
   * any pixels change in Chrome, any pixels in this scaled down version also
   * change. Width scaled down less because we care about
   * small width vertical pixel changes, like a blinking cursor. */
  int scaled_w = round_to_even(this->getWidth() / 2);
  int scaled_h = round_to_even(this->getHeight() / 4);

  NV_IFROGL_TRANSFER_FRAMEBUFFER_FLAGS flags =
      NV_IFROGL_TRANSFER_FRAMEBUFFER_FLAG_SCALE;
  if (XCapture::nvIFR.nvIFROGLTransferFramebufferToSys(
          m_hSysTransferObject, 0, GL_FRONT_LEFT, flags, 0, 0, scaled_w,
          scaled_h) != NV_IFROGL_SUCCESS)
  {
    // log_error("get_raw_frame: Failed to transfer framebuffer data");
    print_nvifr_error();
    nvifrog_force_exit();
    return 0;
  }

  const void *data;
  if (XCapture::nvIFR.nvIFROGLLockTransferData(m_hSysTransferObject, &size,
                                               &data) != NV_IFROGL_SUCCESS)
  {
    // log_error("Failed to lock the transferred raw data");
    nvifrog_force_exit();
    return 0;
  }

  return data;
}

bool XCapture::CaptureSync()
{
  // uintptr_t size = 0;
  // const void *data = get_encoded_frame(size);

  release_last_frame();

  return true;
}

int XErrorFunc(Display *dsp, XErrorEvent *error)
{
  printf("Hit error\n");
  // if (error->error_code == BadMatch)
  // {
  //   // log_error(
  //   //     "Attempt to call glXMakeCurrent() without alpha channel set is "
  //   //     "unsuccessful. Retrying with the alpha channel set.");
  //   return 0;
  // }

  // int errorBase;
  // if (glXQueryExtension(dsp, &errorBase, NULL) &&
  //     error->error_code == errorBase + GLXBadDrawable)
  // {
  //   // log_warn("glXMakeCurrent() failed with GLXBadDrawable.");
  //   glx_make_current_error_count += 1;
  //   if (glx_make_current_error_count > 3)
  //   {
  //     exit(1);
  //   }
  //   return 0;
  // }

  // char errorstring[128];
  // XGetErrorText(dsp, error->error_code, errorstring, 128);
  // log_error("fatal: X error -- <{}>", errorstring);
  // globals::print_globals();

  return 0;
}

std::unique_ptr<XCapture> init_capture(void *userData,
                                       NV_IFROGL_H264_ENC_CONFIG *enc_config)
{
  // CaptureThreadData *threadData = (CaptureThreadData *)userData;
  std::unique_ptr<XCapture> pXCapture = std::make_unique<XCapture>();

  // Try to bind the window without/with specifying the alpha channel property.
  // if (!pXCapture->InitializeGLXContext(threadData->m_pDisplay,
  //                                      threadData->m_iScreen,
  //                                      threadData->m_window, false) &&
  //     !pXCapture->InitializeGLXContext(threadData->m_pDisplay,
  //                                      threadData->m_iScreen,
  //                                      threadData->m_window, true))
  // {
  //   // Both fail. Skip it and keep enumerating other windows.
  //   // log_error(
  //   //     "InitializeGLXContext() failed. Current window ({}) will be skipped.",
  //   //     threadData->m_window);
  //   return nullptr;
  // }
  // pXCapture->InitializeGL(threadData->m_window);
  printf("create enc session\n");
  if (!pXCapture->NvIFROGLCreateEncSession(enc_config))
  {
    // log_error("NvIFROGLCreateEncSession() failed. Program will exit.");
    return nullptr;
  }

  printf("create raw session\n");
  if (!pXCapture->NvIFROGLCreateRawSession())
  {
    // log_error("NvIFROGLCreateRawSession() failed. Program will exit.");
    return nullptr;
  }

  return pXCapture;
}
