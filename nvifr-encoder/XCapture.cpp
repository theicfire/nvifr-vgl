#include "XCapture.h"

NvIFRAPI XCapture::nvIFR;
static NV_IFROGL_HW_ENC_TYPE codecType = NV_IFROGL_HW_ENC_H264;

XCapture::XCapture() : m_fpOut(NULL), m_glxCtx(NULL) {}

XCapture::~XCapture() {
  if (m_fpOut) fclose(m_fpOut);

  // delete NvIFR objects
  if (XCapture::nvIFR.nvIFROGLDestroyTransferObject(m_hTransferObject) !=
      NV_IFROGL_SUCCESS) {
    fprintf(stderr, "Failed to destroy the NvIFROGL transfer object.\n");
    return;
  }

  if (XCapture::nvIFR.nvIFROGLDestroySession(m_hSession) != NV_IFROGL_SUCCESS) {
    fprintf(stderr, "Failed to destroy the NvIFROGL session.\n");
    return;
  }

  if (NULL != m_glxCtx) {
    glXDestroyContext(m_pDisplay, m_glxCtx);
  }
}

void XCapture::NvIFROGLInitialize() {
  if (nvIFR.initialize() == false) {
    fprintf(stderr, "Failed to create NvIFROGL instance.\n");
    exit(-1);
  }
}

bool XCapture::InitializeGLXContext(Display* xDisplay, int screen,
                                    Window window, bool hasAlpha) {
  m_pDisplay = xDisplay;

  int visualAttribs[] = {GLX_RGBA, GLX_ALPHA_SIZE, (int)hasAlpha, None};

  XVisualInfo* visualInfo = glXChooseVisual(m_pDisplay, screen, visualAttribs);
  if (NULL == visualInfo) {
    printf("Creating visual info failed\n");
    return false;
  }

  // Create a GLXContext compatible with window
  m_glxCtx = glXCreateContext(m_pDisplay, visualInfo, NULL, true);
  if (NULL == m_glxCtx) {
    printf("Creating glxContext failed\n");
  }
  XFree(visualInfo);

  // bind GLXContext to the window
  if (!glXMakeCurrent(m_pDisplay, window, m_glxCtx)) {
    glXDestroyContext(m_pDisplay, m_glxCtx);
    return false;
  }

  printf("Initialize GLX context successfully...\n");

  return true;
}

bool XCapture::InitializeGL(Window window) {
  // Get window size
  XWindowAttributes attr;
  // If you do not provide a valid window id, this will just block:
  XGetWindowAttributes(m_pDisplay, window, &attr);
  m_iWidth = attr.width;
  m_iHeight = attr.height;

  // Init OpenGL
  glewInit();
  // cudaGLSetDevice(0);
  glClearColor(0.0, 0.0, 0.0, 1.0);  // 1.0 => A for color conversion matrix
  glClear(GL_COLOR_BUFFER_BIT);
  glViewport(0, 0, m_iWidth, m_iHeight);
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, 1.0, 0.0, 1.0, 0.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glDrawBuffer(GL_BACK);

  return true;
}

bool XCapture::NvIFROGLCreateEncSession(const char* outPath, int fps,
                                        int bitrate,
                                        NV_IFROGL_HW_ENC_TYPE codecType) {
  if (outPath) {
    m_fpOut = fopen(outPath, "wb");
    if (m_fpOut == NULL) {
      fprintf(stderr, "Failed to create output file.\n");
      exit(-1);
    }
  }

  // init nvifr to sys object
  NV_IFROGL_HW_ENC_CONFIG config;

  // A session is required. The session is associated with the current OpenGL
  // context.
  if (XCapture::nvIFR.nvIFROGLCreateSession(&m_hSession, NULL) !=
      NV_IFROGL_SUCCESS) {
    printf("Failed to create a NvIFROGL session.\n");
    return false;
  }

  memset(&config, 0, sizeof(config));

  config.profile = (codecType == NV_IFROGL_HW_ENC_H264) ? 100 : 1;
  config.frameRateNum = fps;
  config.frameRateDen = 1;
  config.width = (m_iWidth + 31) / 32 * 32;
  config.height = (m_iHeight + 31) / 32 * 32;
  if (bitrate >= 0)
    config.avgBitRate = bitrate;
  else
    config.avgBitRate = config.width * config.height * fps * 12 / 8;

  config.GOPLength = 75;
  config.rateControl = NV_IFROGL_HW_ENC_RATE_CONTROL_CBR;
  config.stereoFormat = NV_IFROGL_HW_ENC_STEREO_NONE;
  config.VBVBufferSize = config.avgBitRate;
  config.VBVInitialDelay = config.avgBitRate;
  config.codecType = codecType;

  if (XCapture::nvIFR.nvIFROGLCreateTransferToHwEncObject(
          m_hSession, &config, &m_hTransferObject) != NV_IFROGL_SUCCESS) {
    printf("Failed to create a NvIFROGL transfer object. w=%d, h=%d\n",
           config.width, config.height);
    return false;
  }

  return true;
}

bool XCapture::CaptureSync() {
  uintptr_t dataSize;
  const void* data;

  // transfer the framebuffer
  if (XCapture::nvIFR.nvIFROGLTransferFramebufferToHwEnc(
          m_hTransferObject, NULL, 0, GL_FRONT_LEFT, GL_NONE) !=
      NV_IFROGL_SUCCESS) {
    fprintf(stderr, "Failed to transfer data from the framebuffer.\n");
    exit(-1);
  }

  // lock the transferred data
  if (XCapture::nvIFR.nvIFROGLLockTransferData(m_hTransferObject, &dataSize,
                                               &data) != NV_IFROGL_SUCCESS) {
    fprintf(stderr, "Failed to lock the transferred data.\n");
    return false;
  }

  // write to the file
  if (m_fpOut) fwrite(data, 1, dataSize, m_fpOut);

  // release the data buffer
  if (XCapture::nvIFR.nvIFROGLReleaseTransferData(m_hTransferObject) !=
      NV_IFROGL_SUCCESS) {
    fprintf(stderr, "Failed to release the transferred data.\n");
    return false;
  }

  return true;
}

int XErrorFunc(Display* dsp, XErrorEvent* error) {
  if (error->error_code == BadMatch) {
    fprintf(stderr,
            "Attempt to call glXMakeCurrent() without alpha channel set is "
            "unseccessful. Retrying with the alpha channel set.\n");
    return 0;
  }

  int errorBase;
  if (glXQueryExtension(dsp, &errorBase, NULL) &&
      error->error_code == errorBase + GLXBadDrawable) {
    fprintf(stderr, "Warning: glXMakeCurrent() failed with GLXBadDrawable.\n");
    return 0;
  }

  char errorstring[128];
  XGetErrorText(dsp, error->error_code, errorstring, 128);
  fprintf(stderr, "fatal: X error -- <%s>\n", errorstring);
  exit(-1);
}

void PrintWindowName(Display* pDisplay, Window window) {
  XTextProperty wmName;
  XGetWMName(pDisplay, window, &wmName);

  if (wmName.value && wmName.nitems) {
    int num_prop;
    char** list;
    XmbTextPropertyToTextList(pDisplay, &wmName, &list, &num_prop);
    if (num_prop != 0 && (*list != NULL)) {
      printf("Window ID: %u, Window name: <%s>\n", (unsigned)window, *list);
    }
    XFreeStringList(list);
  }
}

void GetWindowWithGivenName(Display* pDisplay, Window root,
                            vector<Window>* pWindowList,
                            const char* windowName) {
  if (windowName == NULL) {
    pWindowList->push_back(root);
  } else {
    XTextProperty wmName;
    XGetWMName(pDisplay, root, &wmName);

    if (wmName.value && wmName.nitems) {
      int num_prop;
      char** list;
      XmbTextPropertyToTextList(pDisplay, &wmName, &list, &num_prop);
      if (num_prop != 0 && (*list != NULL) &&
          strstr(*list, windowName) != NULL) {
        XWindowAttributes attr;
        XGetWindowAttributes(pDisplay, root, &attr);
        if (attr.width == 0 || attr.height == 0) {
          printf("skipped window: %u, size: %dx%d\n", (unsigned)root,
                 attr.width, attr.height);
        } else {
          pWindowList->push_back(root);
        }
      }

      XFreeStringList(list);
    }
  }

  Window parent;
  Window* children;
  unsigned int numChildren;
  XQueryTree(pDisplay, root, &root, &parent, &children, &numChildren);

  for (unsigned i = 0; i < numChildren; i++) {
    GetWindowWithGivenName(pDisplay, children[i], pWindowList, windowName);
  }

  XFree((char*)children);
}

void GetVisibleWindow(Display* pDisplay, Window root,
                      vector<Window>* pWindowList,
                      const char* windowName = NULL) {
  Atom a = XInternAtom(pDisplay, "_NET_CLIENT_LIST", true);
  Atom actualType;
  int format;
  unsigned long numItems, bytesAfter;
  unsigned char* data;
  XGetWindowProperty(pDisplay, root, a, 0L, (~0L), false, AnyPropertyType,
                     &actualType, &format, &numItems, &bytesAfter, &data);

  Window* array = (Window*)data;
  if (windowName == NULL) {
    for (unsigned i = 0; i < numItems; i++) {
      pWindowList->push_back(array[i]);
    }
  } else {
    for (unsigned i = 0; i < numItems; i++) {
      XTextProperty wmName;
      XGetWMName(pDisplay, array[i], &wmName);

      if (wmName.value && wmName.nitems) {
        int num_prop;
        char** list;
        XmbTextPropertyToTextList(pDisplay, &wmName, &list, &num_prop);
        if (num_prop != 0 && (*list != NULL) &&
            strstr(*list, windowName) != NULL) {
          pWindowList->push_back(array[i]);
        }

        XFreeStringList(list);
      }
    }
  }
}

unsigned int captureThreadSync(void* userData) {
  CaptureThreadData* threadData = (CaptureThreadData*)userData;
  XCapture* pXCapture = new XCapture;

  // Try to bind the window without/with specifying the alpha channel property.
  if (!pXCapture->InitializeGLXContext(threadData->m_pDisplay,
                                       threadData->m_iScreen,
                                       threadData->m_window, false) &&
      !pXCapture->InitializeGLXContext(threadData->m_pDisplay,
                                       threadData->m_iScreen,
                                       threadData->m_window, true)) {
    // Both fail. Skip it and keep enumerating other windows.
    fprintf(stderr,
            "InitializeGLXContext() failed. Current window will be skipped.\n");
    return 1;
  }
  pXCapture->InitializeGL(threadData->m_window);
  if (!pXCapture->NvIFROGLCreateEncSession(strlen(threadData->m_strOutName) > 0
                                               ? threadData->m_strOutName
                                               : NULL,
                                           threadData->m_iFps, 0, codecType)) {
    fprintf(stderr, "NvIFROGLCreateEncSession() failed. Program will exit.");
    exit(-1);
  }

  Timer t;

  while (true) {
    t.reset();
    pXCapture->CaptureSync();
    printf("captured in %.2fms\n", t.getElapsedMilliseconds());
    usleep(33000);
  }
}