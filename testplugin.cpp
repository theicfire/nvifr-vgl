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
#include "Error.h"
// #include "./encoder/encoder.h"
#include "nvifr-encoder/XCapture.h"

using namespace vglutil;

int get_frame_count = 0;

static Error err;
char errStr[MAXSTR + 14], errStrTmp[256];
uint8_t thing[10];
FILE *m_fpOut;

RRFrame rr_frame;
NV_IFROGL_SESSION_HANDLE m_hSession;
static NV_IFROGL_HW_ENC_TYPE codecType = NV_IFROGL_HW_ENC_H264;
NV_IFROGL_TRANSFEROBJECT_HANDLE m_hTransferObject = nullptr;
NV_IFROGL_HW_ENC_CONFIG config;
NV_IFROGL_TO_SYS_CONFIG to_sys_config;
NV_IFROGL_TRANSFEROBJECT_HANDLE m_hSysTransferObject = nullptr;

void throw_nvifr_error(const char *msg)
{
	unsigned int returnedLen, remainingBytes;
	snprintf(errStrTmp, 256, "%s\nnvifrogl error: ", msg);
	XCapture::nvIFR.nvIFROGLGetError(&errStrTmp[strlen(errStrTmp)],
		256 - strlen(errStrTmp), &returnedLen, &remainingBytes);
	THROW(errStrTmp);
}

void setupNVIFRSYS()
{
	to_sys_config.format = NV_IFROGL_TARGET_FORMAT_YUV420P;
	int ret;
	if ((ret = XCapture::nvIFR.nvIFROGLCreateTransferToSysObject(
			 m_hSession, &to_sys_config, &m_hSysTransferObject)) !=
		NV_IFROGL_SUCCESS)
	{
		snprintf(errStrTmp, 256,
			"Lame! NvIFROGLCreateRawSession Failed to create a transfer object "
			"with code %d.", ret);
		THROW(errStrTmp);
	}
}

void setupNVIFRHwEnc(int width, int height)
{
	if (!config.profile) {
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
				m_hSession, &config, &m_hTransferObject) != NV_IFROGL_SUCCESS)
		{
			snprintf(errStrTmp, 256,
				"Failed to create a NvIFROGL transfer object. w=%d, h=%d\n",
				config.width, config.height);
			THROW(errStrTmp);
		}
	}
}

void captureHwEnc()
{
	GLint drawFBO;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFBO);

	GLenum readBuffer;
	glGetIntegerv(GL_READ_BUFFER, (GLint *)&readBuffer);

	fprintf(stderr, "fbo is %d, readBuffer is %d\n", drawFBO, readBuffer);

	uintptr_t dataSize;
	const void *data;

	// transfer the framebuffer
	// if (XCapture::nvIFR.nvIFROGLTransferFramebufferToHwEnc(
	// 		m_hTransferObject, NULL, 0, GL_FRONT_LEFT, GL_NONE) != NV_IFROGL_SUCCESS)
	if (XCapture::nvIFR.nvIFROGLTransferFramebufferToHwEnc(
			m_hTransferObject, NULL, drawFBO, GL_BACK_LEFT, GL_NONE) != NV_IFROGL_SUCCESS)
	{
		throw_nvifr_error("Failed to transfer data from the framebuffer.");
	}

	fprintf(stderr, "Now lock transfer object.\n");

	// lock the transferred data
	if (XCapture::nvIFR.nvIFROGLLockTransferData(m_hTransferObject, &dataSize,
												 &data) != NV_IFROGL_SUCCESS)
	{
		THROW("Failed to lock the transferred data.");
	}

	// write to the file
	if (m_fpOut)
		fwrite(data, 1, dataSize, m_fpOut);

	// release the data buffer
	if (XCapture::nvIFR.nvIFROGLReleaseTransferData(m_hTransferObject) !=
		NV_IFROGL_SUCCESS)
	{
		THROW("Failed to release the transferred data.");
	}
	fprintf(stderr, "Done with captureHwEnc\n");
}

void captureSys()
{
	GLint drawFBO;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFBO);

	GLenum readBuffer;
	glGetIntegerv(GL_READ_BUFFER, (GLint *)&readBuffer);

	fprintf(stderr, "fbo is %d, readBuffer is %d\n", drawFBO, readBuffer);

	uintptr_t dataSize;
	const void *data;

	// transfer the framebuffer
	// if (XCapture::nvIFR.nvIFROGLTransferFramebufferToSys(
	// 		m_hSysTransferObject, 0, GL_FRONT_LEFT, NV_IFROGL_TRANSFER_FRAMEBUFFER_FLAG_NONE, 0, 0, 0, 0) != NV_IFROGL_SUCCESS)
	if (XCapture::nvIFR.nvIFROGLTransferFramebufferToSys(
			m_hSysTransferObject, drawFBO, GL_BACK_LEFT, NV_IFROGL_TRANSFER_FRAMEBUFFER_FLAG_NONE, 0, 0, 0, 0) != NV_IFROGL_SUCCESS)
	{
		throw_nvifr_error("Failed to transfer data from the framebuffer.");
	}

	fprintf(stderr, "Now lock transfer object.\n");

	// lock the transferred data
	if (XCapture::nvIFR.nvIFROGLLockTransferData(m_hSysTransferObject, &dataSize,
												 &data) != NV_IFROGL_SUCCESS)
	{
		THROW("Failed to lock the transferred data.");
	}
	fprintf(stderr, "Done locking transfer object.\n");

	// release the data buffer
	if (XCapture::nvIFR.nvIFROGLReleaseTransferData(m_hSysTransferObject) !=
		NV_IFROGL_SUCCESS)
	{
		THROW("Failed to release the transferred data.");
	}
}

// This just wraps the VGLTrans class in order to demonstrate how to build a
// custom transport plugin for VGL and also to serve as a sanity check for the
// plugin API
extern "C"
{

	void *RRTransInit(Display *dpy, Window win_, FakerConfig *fconfig_)
	{
		fprintf(stderr, "Hit RRTransInit!\n");

		// fprintf(stderr, "Call CreateEncSession\n");

		// setupNVIFRHwEnc();

		memset(&rr_frame, 0, sizeof(RRFrame));
		memset(&config, 0, sizeof(config));

		return (void *)thing;
	}

	int RRTransConnect(void *handle, char *receiverName, int port)
	{
		fprintf(stderr, "RRTransConnect!\n");

		try
		{
			XCapture::nvIFR.initialize();
			if (XCapture::nvIFR.nvIFROGLCreateSession(&m_hSession, NULL) !=
				NV_IFROGL_SUCCESS)
			{
				THROW("Failed to create a NvIFROGL session.");
			}

			m_fpOut = fopen("cool.h264", "wb");
			if (m_fpOut == NULL)
			{
				THROW("Failed to create output file.");
			}

			// setupNVIFRSYS();
		}
		catch(Error &e)
		{
			err = e;  return -1;
		}

		fprintf(stderr, "Done RRTransConnect!\n");
		// Return 0 for success
		return 0;
	}

	RRFrame *RRTransGetFrame(void *handle, int width, int height, int format,
							 int stereo)
	{
		get_frame_count += 1;
		fprintf(stderr, "RRTransGetFrame! w: %d, h: %d, format: %d, stereo: %d. Call count: %d\n", width, height, format, stereo, get_frame_count);
		if (!rr_frame.bits) {
			rr_frame.w = width;
			rr_frame.h = height;
			rr_frame.pitch = width * rrtrans_ps[format];
			rr_frame.bits = new unsigned char[height * rr_frame.pitch];
			rr_frame.rbits = nullptr;
			rr_frame.format = format;
		}
		try
		{
			setupNVIFRHwEnc(width, height);
		}
		catch(Error &e)
		{
			err = e;  return NULL;
		}


		return &rr_frame;
	}

	int RRTransReady(void *handle)
	{
		fprintf(stderr, "RRTransReady!\n");
		// Return 1 for "ready"
		return 1;
	}

	int RRTransSynchronize(void *handle)
	{
		fprintf(stderr, "RRTransSynchronize!\n");
		// Return 0 for success
		return 0;
	}

	int RRTransSendFrame(void *handle, RRFrame *frame, int sync)
	{
		fprintf(stderr, "hmm RRTransSendFrame!\n");

		// A session is required. The session is associated with the current OpenGL
		// context.
		// fprintf(stderr, "Start sleep\n");
		// sleep(10);
		// fprintf(stderr, "End sleep\n");
		// captureSys();
		try
		{
			captureHwEnc();
		}
		catch(Error &e)
		{
			err = e;  return -1;
		}

		fprintf(stderr, "RRTransSendFrame Done!\n");

		// Return 0 for success
		return 0;
	}

	int RRTransDestroy(void *handle)
	{
		fprintf(stderr, "RRTransDestroy!\n");
		delete [] rr_frame.bits;  rr_frame.bits = nullptr;
		// Return 0 for success
		return 0;
	}

	const char *RRTransGetError(void)
	{
		fprintf(stderr, "RRTransGetError!\n");
		snprintf(errStr, MAXSTR + 14, "Error in %s -- %s",
			err.getMethod(), err.getMessage());
		return errStr;
	}

} // extern "C"
