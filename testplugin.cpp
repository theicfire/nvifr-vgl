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
NV_IFROGL_SESSION_HANDLE m_hSession;
static NV_IFROGL_HW_ENC_TYPE codecType = NV_IFROGL_HW_ENC_H264;
NV_IFROGL_TRANSFEROBJECT_HANDLE m_hTransferObject = nullptr;
NV_IFROGL_HW_ENC_CONFIG config;
NV_IFROGL_TO_SYS_CONFIG to_sys_config;
NV_IFROGL_TRANSFEROBJECT_HANDLE m_hSysTransferObject = nullptr;

FakerConfig *fconfig_getinstance(void) { return fconfig; }

void print_nvifr_error()
{
	char errorString[200];
	memset(errorString, 0, 200);
	unsigned int returnedLen, remainingBytes;
	XCapture::nvIFR.nvIFROGLGetError(errorString, 200, &returnedLen, &remainingBytes);
	printf("nvifrogl error: %s", errorString);
}

void setupNVIFRSYS()
{
	to_sys_config.format = NV_IFROGL_TARGET_FORMAT_YUV420P;
	int ret;
	if ((ret = XCapture::nvIFR.nvIFROGLCreateTransferToSysObject(
			 m_hSession, &to_sys_config, &m_hSysTransferObject)) !=
		NV_IFROGL_SUCCESS)
	{
		printf("Lame! NvIFROGLCreateRawSession Failed to create a transfer object "
			   "with code %d. ",
			   ret);
	}
}

void setupNVIFRHwEnc()
{
	memset(&config, 0, sizeof(config));

	config.profile = 100;
	config.frameRateNum = 30;
	config.frameRateDen = 1;
	config.width = 300;
	config.height = 300;
	config.avgBitRate = 300 * 300 * 30 * 12 / 8;

	config.GOPLength = 75;
	config.rateControl = NV_IFROGL_HW_ENC_RATE_CONTROL_CBR;
	config.stereoFormat = NV_IFROGL_HW_ENC_STEREO_NONE;
	config.VBVBufferSize = config.avgBitRate;
	config.VBVInitialDelay = config.avgBitRate;
	config.codecType = codecType;

	if (XCapture::nvIFR.nvIFROGLCreateTransferToHwEncObject(
			m_hSession, &config, &m_hTransferObject) != NV_IFROGL_SUCCESS)
	{
		printf("Failed to create a NvIFROGL transfer object. w=%d, h=%d\n",
			   config.width, config.height);
	}
}

void captureHwEnc()
{
	GLint drawFBO;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFBO);
	printf("fbo is %d\n", drawFBO);

	uintptr_t dataSize;
	const void *data;

	// transfer the framebuffer
	// if (XCapture::nvIFR.nvIFROGLTransferFramebufferToHwEnc(
	// 		m_hTransferObject, NULL, 0, GL_FRONT_LEFT, GL_NONE) != NV_IFROGL_SUCCESS)
	if (XCapture::nvIFR.nvIFROGLTransferFramebufferToHwEnc(
			m_hTransferObject, NULL, drawFBO, GL_COLOR_ATTACHMENT0_EXT, GL_NONE) != NV_IFROGL_SUCCESS)
	{
		fprintf(stderr, "Failed to transfer data from the framebuffer.\n");
		print_nvifr_error();
		exit(-1);
	}


	printf("Now lock transfer object.\n");

	// lock the transferred data
	if (XCapture::nvIFR.nvIFROGLLockTransferData(m_hTransferObject, &dataSize,
												 &data) != NV_IFROGL_SUCCESS)
	{
		fprintf(stderr, "Failed to lock the transferred data.\n");
	}

	printf("FAILS to get here.\n");

	// release the data buffer
	if (XCapture::nvIFR.nvIFROGLReleaseTransferData(m_hTransferObject) !=
		NV_IFROGL_SUCCESS)
	{
		fprintf(stderr, "Failed to release the transferred data.\n");
	}
}

void captureSys()
{
	GLint drawFBO;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFBO);

	GLenum readBuffer;
	glGetIntegerv(GL_READ_BUFFER, (GLint*) &readBuffer);

	printf("fbo is %d, readBuffer is %d\n", drawFBO, readBuffer);

	uintptr_t dataSize;
	const void *data;

	// transfer the framebuffer
	// if (XCapture::nvIFR.nvIFROGLTransferFramebufferToSys(
	// 		m_hSysTransferObject, 0, GL_FRONT_LEFT, NV_IFROGL_TRANSFER_FRAMEBUFFER_FLAG_NONE, 0, 0, 0, 0) != NV_IFROGL_SUCCESS)
	if (XCapture::nvIFR.nvIFROGLTransferFramebufferToSys(
			m_hSysTransferObject, drawFBO, GL_BACK_LEFT, NV_IFROGL_TRANSFER_FRAMEBUFFER_FLAG_NONE, 0, 0, 0, 0) != NV_IFROGL_SUCCESS)
	{
		fprintf(stderr, "Failed to transfer data from the framebuffer.\n");
		print_nvifr_error();
		exit(-1);
	}

	printf("Now lock transfer object.\n");

	// lock the transferred data
	if (XCapture::nvIFR.nvIFROGLLockTransferData(m_hSysTransferObject, &dataSize,
												 &data) != NV_IFROGL_SUCCESS)
	{
		fprintf(stderr, "Failed to lock the transferred data.\n");
	}
	printf("Done locking transfer object.\n");

	// release the data buffer
	if (XCapture::nvIFR.nvIFROGLReleaseTransferData(m_hSysTransferObject) !=
		NV_IFROGL_SUCCESS)
	{
		fprintf(stderr, "Failed to release the transferred data.\n");
	}
}

// This just wraps the VGLTrans class in order to demonstrate how to build a
// custom transport plugin for VGL and also to serve as a sanity check for the
// plugin API
extern "C"
{

	void *RRTransInit(Display *dpy, Window win_, FakerConfig *fconfig_)
	{
		printf("Hit RRTransInit!\n");

		// printf("Call CreateEncSession\n");

		// setupNVIFRHwEnc();

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
		printf("hmm RRTransSendFrame!\n");

		// A session is required. The session is associated with the current OpenGL
		// context.
		// printf("Start sleep\n");
		// sleep(10);
		// printf("End sleep\n");
		XCapture::nvIFR.initialize();
		if (XCapture::nvIFR.nvIFROGLCreateSession(&m_hSession, NULL) !=
			NV_IFROGL_SUCCESS)
		{
			printf("Failed to create a NvIFROGL session.\n");
			return 0;
		}
		setupNVIFRSYS();
		captureSys();
		printf("RRTransSendFrame Done!\n");

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
