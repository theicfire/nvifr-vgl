/**
* Run this inside of virtualgl-2.6.3/server:
* g++ -I../include/ -I../common/ -I/opt/libjpeg-turbo/include/ -I../build/include/  -c -Wall -fpic testplugin.cpp
* g++ -shared -o ~/libvgltrans_hello.so testplugin.o
* vglrun -v -trans hello -d :1 glxgears # Not this is a random 3D program installed by default on Ubuntu
*/

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include "rrtransport.h"
#include "../virtualgl-2.6.3/server/VGLTrans.h"
// #include "./encoder/encoder.h"
#include "nvifr-encoder/XCapture.h"

using namespace vglutil;
using namespace vglcommon;
using namespace vglserver;

int get_frame_count = 0;
static NV_IFROGL_HW_ENC_TYPE codecType = NV_IFROGL_HW_ENC_H264;

static Error err;
char errStr[MAXSTR + 14];
unsigned char rrframe_bits[300 * 300 * 3];
uint8_t thing[10];

static FakerConfig *fconfig = NULL;

static const int trans2pf[RRTRANS_FORMATOPT] =
	{
		PF_RGB, PF_RGBX, PF_BGR, PF_BGRX, PF_XBGR, PF_XRGB};

static const int pf2trans[PIXELFORMATS] =
	{
		RRTRANS_RGB, RRTRANS_RGBA, RRTRANS_BGR, RRTRANS_BGRA, RRTRANS_ABGR,
		RRTRANS_ARGB, RRTRANS_RGB};

RRFrame rr_frame;

FakerConfig *fconfig_getinstance(void) { return fconfig; }

// This just wraps the VGLTrans class in order to demonstrate how to build a
// custom transport plugin for VGL and also to serve as a sanity check for the
// plugin API

extern "C"
{

	void *RRTransInit(Display *dpy, Window win_, FakerConfig *fconfig_)
	{
		printf("ooo RRTransInit!\n");
		string window_name;
		string out_file = "window.h264";

		window_name = "YouTube - Google Chrome";

		Display *display = XOpenDisplay(NULL);
		if (display == NULL)
		{
			printf("Unable to open display.\n");
			return (void *)thing;
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
			 ++i)
		{
			PrintWindowName(display, *i);
		}

		if (window_name.size() > 0)
		{
			XCapture::NvIFROGLInitialize();
			CaptureThreadData thread_data;
			thread_data.m_pDisplay = display;
			thread_data.m_iScreen = screen;
			thread_data.m_iFps = 30;
			thread_data.m_window = windowList[0];
			sprintf(thread_data.m_strOutName, "%s", out_file.c_str());

			CaptureThreadData *threadData = (CaptureThreadData *)&thread_data;
			XCapture *pXCapture = new XCapture;

			// Try to bind the window without/with specifying the alpha channel property.
			if (!pXCapture->InitializeGLXContext(threadData->m_pDisplay,
												 threadData->m_iScreen,
												 threadData->m_window, false) &&
				!pXCapture->InitializeGLXContext(threadData->m_pDisplay,
												 threadData->m_iScreen,
												 threadData->m_window, true))
			{
				// Both fail. Skip it and keep enumerating other windows.
				fprintf(stderr,
						"InitializeGLXContext() failed. Current window will be skipped.\n");
				return (void *)thing;
			}
			pXCapture->InitializeGL(threadData->m_window);
			printf("Call CreateEncSession\n");
			if (!pXCapture->NvIFROGLCreateEncSession(strlen(threadData->m_strOutName) > 0
														 ? threadData->m_strOutName
														 : NULL,
													 threadData->m_iFps, 0, codecType))
			{
				fprintf(stderr, "NvIFROGLCreateEncSession() failed. Program will exit.");
				exit(-1);
			}
			printf("Done CreateEncSession\n");
		}

		return (void *)thing;
	}

	int RRTransConnect(void *handle, char *receiverName, int port)
	{
		printf("RRTransConnect!\n");
		// Return 0 for success
		return 0;
	}

	RRFrame *RRTransGetFrame(void *handle, int width, int height, int format,
							 int stereo)
	{
		get_frame_count += 1;
		printf("RRTransGetFrame! w: %d, h: %d, format: %d, stereo: %d. Call count: %d\n", width, height, format, stereo, get_frame_count);
		rr_frame.bits = rrframe_bits;
		rr_frame.rbits = nullptr;
		rr_frame.format = format;
		rr_frame.w = width;
		rr_frame.h = height;
		// TODO this * 3 is a guess! 1 byte for r/g/b? I dunno!
		rr_frame.pitch = width * 3;
		// kinda rorrect.. should work I think..
		return &rr_frame;
	}

	int RRTransReady(void *handle)
	{
		printf("RRTransReady!\n");
		// Return 1 for "ready"
		return 1;
	}

	int RRTransSynchronize(void *handle)
	{
		printf("RRTransSynchronize!\n");
		// Return 0 for success
		return 0;
	}

	int RRTransSendFrame(void *handle, RRFrame *frame, int sync)
	{
		printf("RRTransSendFrame!\n");
		// Return 0 for success
		return 0;
	}

	int RRTransDestroy(void *handle)
	{
		printf("RRTransDestroy!\n");
		// Return 0 for success
		return 0;
	}

	const char *RRTransGetError(void)
	{
		printf("RRTransGetError!\n");
		snprintf(errStr, MAXSTR + 14, "Error in %s -- %s",
				 err.getMethod(), err.getMessage());
		return errStr;
	}

} // extern "C"
