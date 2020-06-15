// This file contains code based on the VirtualGL example image transport
// plugins and built-in image transports.  This code is:
//
// Copyright (C)2009-2011, 2014, 2017-2020 D. R. Commander
//
// The code in question has been re-licensed, with permission of the author,
// from the wxWidgets Library License v3.1 to the business-friendly BSD-style
// license listed below.  All other code contributions in this file from the
// same author retain the same copyright and are provided under the same
// license.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// - Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// - Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// - Neither the name of The VirtualGL Project nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS",
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include <X11/Xlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "Error.h"
#include "GenericQ.h"
#include "Thread.h"
#include "rrtransport.h"
#include "shared_mem_comm.h"

#define GL_GLEXT_PROTOTYPES
#include "nvifr-encoder/XCapture.h"

extern "C" void _vgl_disableFaker(void) __attribute__((weak));
extern "C" void _vgl_enableFaker(void) __attribute__((weak));

using vglutil::CriticalSection;
using vglutil::Error;
using vglutil::Event;
using vglutil::GenericQ;
using vglutil::Runnable;
using vglutil::Thread;

static __thread char errStr[MAXSTR + 14];
static NV_IFROGL_HW_ENC_TYPE codecType = NV_IFROGL_HW_ENC_H264;

FILE *debug = nullptr;

void log_format(const char *tag, const char *message, va_list args) {
  if (debug == nullptr) {
    return;
  }
  time_t now;
  time(&now);
  char *date = ctime(&now);
  date[strlen(date) - 1] = '\0';
  fprintf(debug, "%s [%s] ", date, tag);
  vfprintf(debug, message, args);
  fprintf(debug, "\n");
  fflush(debug);
}

void log_info(const char *message, ...) {
  va_list args;
  va_start(args, message);
  log_format("info", message, args);
  va_end(args);
}

class GPUEncBuffer {
 public:
  GPUEncBuffer(void)
      : fbo(0),
        rbo(0),
        width(0),
        height(0),
        dpy3D(NULL),
        draw(0),
        read(0),
        vglCtx(0),
        ctx(0) {
    ready.wait();

    dpy3D = glXGetCurrentDisplay();
    draw = glXGetCurrentDrawable();
    read = glXGetCurrentReadDrawable();
    vglCtx = glXGetCurrentContext();
    if (!dpy3D || !draw || !read || !vglCtx)
      THROW("OpenGL context created by VirtualGL Faker is invalid");
    int fbcid = 0;
    glXQueryContext(dpy3D, vglCtx, GLX_FBCONFIG_ID, &fbcid);
    if (!fbcid) THROW("OpenGL context created by VirtualGL Faker is invalid");
    int attribs[] = {GLX_FBCONFIG_ID, fbcid, None}, n = 0;
    GLXFBConfig *configs =
        glXChooseFBConfig(dpy3D, DefaultScreen(dpy3D), attribs, &n);
    if (!configs || n < 1)
      THROW("Could not obtain FB config from current context.");
    GLXFBConfig config = configs[0];
    XFree(configs);
    if (!(ctx = glXCreateNewContext(dpy3D, config, GLX_RGBA_TYPE, NULL, True)))
      THROW("Could not create OpenGL context for GPU buffer");
  }

  ~GPUEncBuffer(void) {
    destroy();
    makeCurrentVGL();
    glXDestroyContext(dpy3D, ctx);
  }

  void destroy(void) {
    makeCurrent(dpy3D);
    if (rbo) glDeleteRenderbuffers(1, &rbo);
    if (fbo) glDeleteFramebuffers(1, &fbo);
  }

  void init(int width_, int height_) {
    if (fbo && rbo && width_ == width && height_ == height) return;

    width = width_;
    height = height_;
    destroy();
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, rbo);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glBindRenderbuffer(GL_RENDERBUFFER, 0);
      makeCurrentVGL();
      THROW("FBO is incomplete");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    makeCurrentVGL();
  }

  void makeCurrent(Display *dpy) {
    if (!glXMakeContextCurrent(dpy, draw, read, ctx))
      THROW("Could not make GPU buffer's OpenGL context current");
  }

  void makeCurrentVGL(void) {
    if (!glXMakeContextCurrent(dpy3D, draw, read, vglCtx))
      THROW("Could not make VirtualGL's OpenGL context current");
  }

  void copyPixels(void) {
    GLint readBuf = 0;
    glGetIntegerv(GL_READ_BUFFER, &readBuf);
    if (!readBuf) THROW("Could not get current read buffer");
    makeCurrent(dpy3D);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, rbo);
    if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE) {
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
      glBindRenderbuffer(GL_RENDERBUFFER, 0);
      makeCurrentVGL();
      THROW("FBO is incomplete");
    }
    glReadBuffer(readBuf);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glCopyPixels(0, 0, width, height, GL_COLOR);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    makeCurrentVGL();
  }

  GLuint getFBO(void) { return fbo; }
  GLuint getRBO(void) { return rbo; }

  void signalComplete(void) { complete.signal(); }
  void waitComplete(void) { complete.wait(); }
  bool isComplete(void) { return !complete.isLocked(); }

 private:
  Event ready, complete;  // wait: *decrement semaphore*, isLocked: check. Don't
                          // wait or decrement., signal: *increment semaphore*
  GLuint fbo, rbo;
  int width, height;
  Display *dpy3D;
  GLXDrawable draw, read;
  GLXContext vglCtx, ctx;
};

class GPUEncTrans : public Runnable {
 public:
  GPUEncTrans(Window win_, FakerConfig *fconfig_)
      : m_fpOut(NULL),
        m_hSession(NULL),
        m_hTransferObject(NULL),
        m_hSysTransferObject(NULL),
        win(win_),
        fconfig(fconfig_),
        alreadyWarnedRenderMode(false),
        shutdown(false),
        thread(NULL),
        width(0),
        height(0),
        dpy3DClone(NULL),
        mighty_ipc(false) {
    memset(rr_frame, 0, sizeof(RRFrame) * NFRAMES);
    for (int i = 0; i < NFRAMES; i++) rr_frame[i].opaque = (void *)&buf[i];

    Display *dpy3D = glXGetCurrentDisplay();
    if (!dpy3D) THROW("OpenGL context created by VirtualGL Faker is invalid");
    if (!(dpy3DClone = XOpenDisplay(DisplayString(dpy3D))))
      THROW("Could not clone 3D X server connection");

    thread = new Thread(this);
    thread->start();
  }

  ~GPUEncTrans() {
    shutdown = true;
    queue.release();
    if (thread) {
      thread->stop();
      delete thread;
    }
    if (m_fpOut) fclose(m_fpOut);
    if (dpy3DClone) XCloseDisplay(dpy3DClone);
  }

  RRFrame *getFrame(int width, int height, int format) {
    RRFrame *frame;
    if (shutdown) return NULL;
    if (thread) thread->checkError();
    {
      CriticalSection::SafeLock l(mutex);

      int index = -1;
      for (int i = 0; i < NFRAMES; i++)
        if (buf[i].isComplete())  // we could break; here?
          index = i;
      if (index < 0) THROW("No free buffers in pool");
      frame = &rr_frame[index];
      ((GPUEncBuffer *)frame->opaque)->waitComplete();
    }
    frame->w = width;
    frame->h = height;
    frame->pitch = width * rrtrans_ps[format];
    frame->bits = nullptr;
    frame->rbits = nullptr;
    frame->format = format;
    ((GPUEncBuffer *)frame->opaque)->init(width, height);
    return frame;
  }

  bool isReady(void) {
    if (thread) thread->checkError();
    return queue.items() <= 0;
  }

  void synchronize(void) {
    log_info(
        "ERROR: Synchronize should not be called because frame spoiling is "
        "enabled");
    ready.wait();
  }

  void sendFrame(RRFrame *frame) {
    if (thread) thread->checkError();
    ((GPUEncBuffer *)frame->opaque)->copyPixels();

    // Add frame. Delete everything, and callback to spoilFunction for all
    // deleted frames.
    // TODO what if this is called while something is being encoded? There's
    // nothing to stop us from saying "this frame is completed!"
    queue.spoil((void *)frame, spoilFunction);
  }

  void setupNVIFRSYS();
  void setupNVIFRHwEnc(int width, int height);
  void captureHwEnc(GLuint fbo, GLuint rbo);
  void captureSys(GLuint fbo, GLuint rbo);
  void reset_encoder(uint64_t shared_mem_id);

 private:
  void throw_nvifr_error(const char *function, int line);
  void initNVIFR();

  void run(void);

  static void spoilFunction(void *queuedObject) {
    RRFrame *frame = (RRFrame *)queuedObject;
    if (frame) ((GPUEncBuffer *)frame->opaque)->signalComplete();
  }

  char errStrTmp[256];
  FILE *m_fpOut;
  static const int NFRAMES = 3;
  RRFrame rr_frame[NFRAMES];
  NV_IFROGL_SESSION_HANDLE m_hSession;
  NV_IFROGL_TRANSFEROBJECT_HANDLE m_hTransferObject;
  NV_IFROGL_TRANSFEROBJECT_HANDLE m_hSysTransferObject;
  Window win;
  FakerConfig *fconfig;
  bool alreadyWarnedRenderMode;
  CriticalSection mutex;
  GPUEncBuffer buf[NFRAMES];
  bool shutdown;
  Event ready;
  GenericQ queue;
  Thread *thread;
  int width, height;
  Display *dpy3DClone;
  std::unique_ptr<SharedMem> shared_mem;
  VglMightyIPC mighty_ipc;
};

void GPUEncTrans::run(void) {
  _vgl_disableFaker();

  try {
    while (!shutdown) {
      void *ptr = NULL;
      log_info("Waiting for frame request..");
      VglRPC rpc = mighty_ipc.receive();
      if (rpc.id == VglRPCId::RESTART) {
        log_info("Restart requested! Id: %d", rpc.shared_mem_id);
        reset_encoder(rpc.shared_mem_id);
      }
      log_info("Got frame request!");
      if (queue.items() == 0) {
        VglRPC res = {.id = VglRPCId::EMPTY_RESPONSE};
        mighty_ipc.send(res);
      } else {
        queue.get(&ptr);  // dequeue
        RRFrame *frame = (RRFrame *)ptr;
        if (shutdown) return;
        if (!frame) THROW("Queue has been shut down");
        ready.signal();

        GPUEncBuffer *buf = (GPUEncBuffer *)frame->opaque;
        buf->makeCurrent(dpy3DClone);
        // setupNVIFRSYS()
        setupNVIFRHwEnc(frame->w, frame->h);
        // captureSys(buf->getFBO());
        captureHwEnc(buf->getFBO(), buf->getRBO());
        glXMakeContextCurrent(dpy3DClone, 0, 0, 0);

        // A buffer is complete either when it is spoiled or when it has been
        // encoded.
        buf->signalComplete();
        log_info("Sending frame response.. with size %d",
                 shared_mem->get_written_size());
        VglRPC res = {.id = VglRPCId::FRAME_RESPONSE};
        mighty_ipc.send(res);
      }
    }
  } catch (Error &e) {
    glXMakeContextCurrent(dpy3DClone, 0, 0, 0);
    if (thread) thread->setError(e);
    if (m_hTransferObject)
      XCapture::nvIFR.nvIFROGLDestroyTransferObject(m_hTransferObject);
    if (m_hSysTransferObject)
      XCapture::nvIFR.nvIFROGLDestroyTransferObject(m_hSysTransferObject);
    if (m_hSession) XCapture::nvIFR.nvIFROGLDestroySession(m_hSession);
    ready.signal();
    _vgl_enableFaker();
    throw;
  }
  glXMakeContextCurrent(dpy3DClone, 0, 0, 0);
  if (m_hTransferObject)
    XCapture::nvIFR.nvIFROGLDestroyTransferObject(m_hTransferObject);
  if (m_hSysTransferObject)
    XCapture::nvIFR.nvIFROGLDestroyTransferObject(m_hSysTransferObject);
  if (m_hSession) XCapture::nvIFR.nvIFROGLDestroySession(m_hSession);
  _vgl_enableFaker();
}

void GPUEncTrans::throw_nvifr_error(const char *function, int line) {
  unsigned int returnedLen, remainingBytes;
  XCapture::nvIFR.nvIFROGLGetError(errStrTmp, 256, &returnedLen,
                                   &remainingBytes);
  throw(vglutil::Error(function, errStrTmp, line));
}

void GPUEncTrans::initNVIFR() {
  if (m_hSession) return;

  XCapture::nvIFR.initialize();
  if (XCapture::nvIFR.nvIFROGLCreateSession(&m_hSession, NULL) !=
      NV_IFROGL_SUCCESS) {
    THROW("Failed to create a NvIFROGL session.");
  }
}

void GPUEncTrans::setupNVIFRSYS() {
  initNVIFR();

  if (m_hSysTransferObject) return;

  NV_IFROGL_TO_SYS_CONFIG to_sys_config;
  memset(&to_sys_config, 0, sizeof(NV_IFROGL_TO_SYS_CONFIG));
  to_sys_config.format = NV_IFROGL_TARGET_FORMAT_YUV420P;
  int ret;
  if ((ret = XCapture::nvIFR.nvIFROGLCreateTransferToSysObject(
           m_hSession, &to_sys_config, &m_hSysTransferObject)) !=
      NV_IFROGL_SUCCESS) {
    throw_nvifr_error("nvIFROGLCreateTransferToSysObject()", __LINE__);
  }
}

void GPUEncTrans::reset_encoder(uint64_t shared_mem_id) {
  if (m_hTransferObject) {
    XCapture::nvIFR.nvIFROGLDestroyTransferObject(m_hTransferObject);
  }
  m_hTransferObject = NULL;

  if (m_fpOut) {
    fclose(m_fpOut);
  }
  std::stringstream ss;
  ss << "window-" << shared_mem_id << ".h264";
  log_info("new file: %s", ss.str().c_str());
  m_fpOut = fopen(ss.str().c_str(), "wb");
  if (m_fpOut == NULL) {
    THROW("Failed to create output file.");
  }

  shared_mem = std::make_unique<SharedMem>(false, shared_mem_id);
}

void GPUEncTrans::setupNVIFRHwEnc(int width_, int height_) {
  initNVIFR();

  if (m_hTransferObject && width_ == width && height_ == height) return;

  width = width_;
  height = height_;

  if (m_hTransferObject)
    XCapture::nvIFR.nvIFROGLDestroyTransferObject(m_hTransferObject);

  NV_IFROGL_HW_ENC_CONFIG config;
  memset(&config, 0, sizeof(NV_IFROGL_HW_ENC_CONFIG));
  config.profile = 100;
  config.frameRateNum = 30;
  config.frameRateDen = 1;
  config.width = width;
  config.height = height;
  config.avgBitRate = width * height * 30 * 12 / 8;

  config.GOPLength = 75;
  config.rateControl = NV_IFROGL_HW_ENC_RATE_CONTROL_CBR;
  config.stereoFormat = NV_IFROGL_HW_ENC_STEREO_NONE;
  config.VBVBufferSize = config.avgBitRate;
  config.VBVInitialDelay = config.avgBitRate;
  config.codecType = codecType;

  if (XCapture::nvIFR.nvIFROGLCreateTransferToHwEncObject(
          m_hSession, &config, &m_hTransferObject) != NV_IFROGL_SUCCESS) {
    throw_nvifr_error("nvIFROGLCreateTransferToHwEncObject()", __LINE__);
  }
}

void GPUEncTrans::captureHwEnc(GLuint fbo, GLuint rbo) {
  int renderMode = 0;
  glGetIntegerv(GL_RENDER_MODE, &renderMode);
  if (renderMode != GL_RENDER && renderMode != 0) {
    if (!alreadyWarnedRenderMode && fconfig->verbose) {
      fprintf(stderr,
              "WARNING: One or more readbacks skipped because render mode != "
              "GL_RENDER.\n");
      alreadyWarnedRenderMode = true;
    }
    return;
  }

  try {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, rbo);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      THROW("FBO is incomplete");

    uintptr_t dataSize;
    const void *data;

    if (XCapture::nvIFR.nvIFROGLTransferFramebufferToHwEnc(
            m_hTransferObject, NULL, fbo, GL_COLOR_ATTACHMENT0, GL_NONE) !=
        NV_IFROGL_SUCCESS) {
      throw_nvifr_error("nvIFROGLTransferFramebufferToHwEnc()", __LINE__);
    }

    // lock the transferred data
    if (XCapture::nvIFR.nvIFROGLLockTransferData(m_hTransferObject, &dataSize,
                                                 &data) != NV_IFROGL_SUCCESS) {
      throw_nvifr_error("nvIFROGLLockTransferData()", __LINE__);
    }

    // write to the file
    if (m_fpOut) fwrite(data, 1, dataSize, m_fpOut);

    shared_mem->write((uint8_t *)data, dataSize);

    // release the data buffer
    if (XCapture::nvIFR.nvIFROGLReleaseTransferData(m_hTransferObject) !=
        NV_IFROGL_SUCCESS) {
      throw_nvifr_error("nvIFROGLReleaseTransferData()", __LINE__);
    }
  } catch (...) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    throw;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void GPUEncTrans::captureSys(GLuint fbo, GLuint rbo) {
  int renderMode = 0;
  glGetIntegerv(GL_RENDER_MODE, &renderMode);
  if (renderMode != GL_RENDER && renderMode != 0) {
    if (!alreadyWarnedRenderMode && fconfig->verbose) {
      fprintf(stderr,
              "WARNING: One or more readbacks skipped because render mode != "
              "GL_RENDER.\n");
      alreadyWarnedRenderMode = true;
    }
    return;
  }

  try {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, rbo);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      THROW("FBO is incomplete");

    uintptr_t dataSize;
    const void *data;

    if (XCapture::nvIFR.nvIFROGLTransferFramebufferToSys(
            m_hSysTransferObject, fbo, GL_COLOR_ATTACHMENT0,
            NV_IFROGL_TRANSFER_FRAMEBUFFER_FLAG_NONE, 0, 0, 0,
            0) != NV_IFROGL_SUCCESS) {
      throw_nvifr_error("nvIFROGLTransferFramebufferToSys()", __LINE__);
    }

    // lock the transferred data
    if (XCapture::nvIFR.nvIFROGLLockTransferData(
            m_hSysTransferObject, &dataSize, &data) != NV_IFROGL_SUCCESS) {
      throw_nvifr_error("nvIFROGLLockTransferData()", __LINE__);
    }

    // release the data buffer
    if (XCapture::nvIFR.nvIFROGLReleaseTransferData(m_hSysTransferObject) !=
        NV_IFROGL_SUCCESS) {
      throw_nvifr_error("nvIFROGLReleaseTransferData()", __LINE__);
    }
  } catch (...) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    throw;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

extern "C" {

void *RRTransInit(Display *dpy, Window win, FakerConfig *fconfig) {
  _vgl_disableFaker();

  if (debug == nullptr) {
    debug = fopen("/tmp/vgl.log", "a");
  }
  log_info("Call RRTransInit");

  void *handle = NULL;
  try {
    handle = (void *)(new GPUEncTrans(win, fconfig));
  } catch (Error &e) {
    snprintf(errStr, MAXSTR + 14, "Error in %s -- %s", e.getMethod(),
             e.getMessage());
    handle = NULL;
  }

  _vgl_enableFaker();

  return handle;
}

int RRTransConnect(void *handle, char *receiverName, int port) { return 0; }

RRFrame *RRTransGetFrame(void *handle, int width, int height, int format,
                         int stereo) {
  log_info("RRTransGetFrame()");
  _vgl_disableFaker();

  RRFrame *frame = NULL;
  try {
    GPUEncTrans *trans = (GPUEncTrans *)handle;
    if (!trans) THROW("Invalid handle");
    frame = trans->getFrame(width, height, format);
  } catch (Error &e) {
    snprintf(errStr, MAXSTR + 14, "Error in %s -- %s", e.getMethod(),
             e.getMessage());
    frame = NULL;
  }

  _vgl_enableFaker();

  return frame;
}

int RRTransReady(void *handle) {
  _vgl_disableFaker();

  log_info("Error: RRTransReady() called. Spoil last may be happening");
  int ret = -1;
  try {
    GPUEncTrans *trans = (GPUEncTrans *)handle;
    if (!trans) THROW("Invalid handle");
    ret = (int)trans->isReady();
  } catch (Error &e) {
    snprintf(errStr, MAXSTR + 14, "Error in %s -- %s", e.getMethod(),
             e.getMessage());
    ret = -1;
  }

  _vgl_enableFaker();

  return ret;
}

int RRTransSynchronize(void *handle) {
  _vgl_disableFaker();

  int ret = 0;
  try {
    GPUEncTrans *trans = (GPUEncTrans *)handle;
    if (!trans) THROW("Invalid handle");
    trans->synchronize();
  } catch (Error &e) {
    snprintf(errStr, MAXSTR + 14, "Error in %s -- %s", e.getMethod(),
             e.getMessage());
    ret = -1;
  }

  _vgl_enableFaker();

  return ret;
}

int RRTransSendFrame(void *handle, RRFrame *frame, int sync) {
  log_info("RRTransSendFrame()");
  _vgl_disableFaker();

  int ret = 0;
  try {
    GPUEncTrans *trans = (GPUEncTrans *)handle;
    if (!trans) THROW("Invalid handle");
    if (!frame || !frame->opaque) THROW("Invalid frame handle");
    // NOTE: This plugin does not support strict 2D/3D synchronization (refer
    // to the description of VGL_SYNC in the User's Guide), nor could it.
    // That mode is intended only for rare applications that require strict
    // synchronization between OpenGL and X11, e.g. applications that call
    // XGetImage() to obtain the pixels from a rendered frame immediately
    // after calling glXSwapBuffers() or glXWaitGL().  Supporting such
    // applications requires an image transport that draws images to the 2D X
    // server.  Fortunately, Chrome is not such an application.
    trans->sendFrame(frame);
  } catch (Error &e) {
    snprintf(errStr, MAXSTR + 14, "Error in %s -- %s", e.getMethod(),
             e.getMessage());
    ret = -1;
  }

  _vgl_enableFaker();

  return ret;
}

int RRTransDestroy(void *handle) {
  _vgl_disableFaker();

  fclose(debug);
  int ret = 0;
  try {
    GPUEncTrans *trans = (GPUEncTrans *)handle;
    if (!trans) THROW("Invalid handle");
    delete trans;
  } catch (Error &e) {
    snprintf(errStr, MAXSTR + 14, "Error in %s -- %s", e.getMethod(),
             e.getMessage());
    ret = -1;
  }

  _vgl_enableFaker();

  return ret;
}

const char *RRTransGetError(void) { return errStr; }

}  // extern "C"
