#include "XCapture.h"

int fps = 30;

string window_name;
string out_file = "window.h264";

int main(int argc, char* argv[]) {
  window_name = "YouTube - Google Chrome";

  Display* display = XOpenDisplay(NULL);
  if (display == NULL) {
    printf("Unable to open display.\n");
    return -1;
  }
  int screen = DefaultScreen(display);
  printf("Screen: %d\n", screen);

  XSetErrorHandler(XErrorFunc);
  Window root_window = XDefaultRootWindow(display);

  printf("Enumerate windows: \n");
  vector<Window> windowList;
  GetWindowWithGivenName(display, root_window, &windowList,
                         window_name.c_str());
  for (vector<Window>::iterator i = windowList.begin(); i != windowList.end();
       ++i) {
    PrintWindowName(display, *i);
  }

  if (window_name.size() > 0) {
    XCapture::NvIFROGLInitialize();
    CaptureThreadData thread_data;
    thread_data.m_pDisplay = display;
    thread_data.m_iScreen = screen;
    thread_data.m_iFps = fps;
    thread_data.m_window = windowList[0];
    sprintf(thread_data.m_strOutName, "%s", out_file.c_str());

    captureThreadSync(&thread_data);
  }

  return 0;
}