// This file contains code based on the VirtualGL example image transport
// plugins, which are:
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

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include "rrtransport.h"
#include "Error.h"
#include "nvifr-encoder/XCapture.h"

extern "C" void _vgl_disableFaker(void) __attribute__((weak));
extern "C" void _vgl_enableFaker(void) __attribute__((weak));

using namespace vglutil;

static __thread char errStr[MAXSTR + 14];
static NV_IFROGL_HW_ENC_TYPE codecType = NV_IFROGL_HW_ENC_H264;

class GPUEncTrans
{
	public:
		GPUEncTrans(Display *dpy_, Window win_, FakerConfig *fconfig_) :
			m_fpOut(NULL), m_hSession(NULL), m_hTransferObject(NULL),
			m_hSysTransferObject(NULL), dpy(dpy_), win(win_), fconfig(fconfig_),
			alreadyWarnedRenderMode(false)
		{
			memset(&rr_frame, 0, sizeof(RRFrame));
			char tmp[256];
			snprintf(tmp, 256, "%lx.h264", win);
			m_fpOut = fopen(tmp, "wb");
			if (m_fpOut == NULL)
			{
				THROW("Failed to create output file.");
			}
		}

		~GPUEncTrans()
		{
			if (m_hTransferObject)
				XCapture::nvIFR.nvIFROGLDestroyTransferObject(m_hTransferObject);
			if (m_hSysTransferObject)
				XCapture::nvIFR.nvIFROGLDestroyTransferObject(m_hSysTransferObject);
			if (m_hSession) XCapture::nvIFR.nvIFROGLDestroySession(m_hSession);
			if (m_fpOut) fclose(m_fpOut);
		}

		RRFrame *getFrame(int width, int height, int format)
		{
			rr_frame.w = width;
			rr_frame.h = height;
			rr_frame.pitch = width * rrtrans_ps[format];
			rr_frame.bits = nullptr;
			rr_frame.rbits = nullptr;
			rr_frame.format = format;
			return &rr_frame;
		}

		void setupNVIFRSYS();
		void setupNVIFRHwEnc(int width, int height);
		void captureHwEnc();
		void captureSys();

	private:
		void throw_nvifr_error(const char *msg);
		void initNVIFR();

		char errStrTmp[256];
		FILE *m_fpOut;
		RRFrame rr_frame;
		NV_IFROGL_SESSION_HANDLE m_hSession;
		NV_IFROGL_TRANSFEROBJECT_HANDLE m_hTransferObject;
		NV_IFROGL_TRANSFEROBJECT_HANDLE m_hSysTransferObject;
		Display *dpy;
		Window win;
		FakerConfig *fconfig;
		bool alreadyWarnedRenderMode;
};

void GPUEncTrans::throw_nvifr_error(const char *msg)
{
	unsigned int returnedLen, remainingBytes;
	snprintf(errStrTmp, 256, "%s\nnvifrogl error: ", msg);
	XCapture::nvIFR.nvIFROGLGetError(&errStrTmp[strlen(errStrTmp)],
		256 - strlen(errStrTmp), &returnedLen, &remainingBytes);
	THROW(errStrTmp);
}

void GPUEncTrans::initNVIFR()
{
	if (m_hSession) return;

	XCapture::nvIFR.initialize();
	if (XCapture::nvIFR.nvIFROGLCreateSession(&m_hSession, NULL) !=
		NV_IFROGL_SUCCESS)
	{
		THROW("Failed to create a NvIFROGL session.");
	}
}

void GPUEncTrans::setupNVIFRSYS()
{
	initNVIFR();

	if (m_hSysTransferObject) return;

	NV_IFROGL_TO_SYS_CONFIG to_sys_config;
	memset(&to_sys_config, 0, sizeof(NV_IFROGL_TO_SYS_CONFIG));
	to_sys_config.format = NV_IFROGL_TARGET_FORMAT_YUV420P;
	int ret;
	if ((ret = XCapture::nvIFR.nvIFROGLCreateTransferToSysObject(m_hSession,
		&to_sys_config, &m_hSysTransferObject)) != NV_IFROGL_SUCCESS)
	{
		snprintf(errStrTmp, 256,
			"Lame! NvIFROGLCreateRawSession Failed to create a transfer object "
			"with code %d.", ret);
		THROW(errStrTmp);
	}
}

void GPUEncTrans::setupNVIFRHwEnc(int width, int height)
{
	initNVIFR();

	if (m_hTransferObject) return;

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

	if (XCapture::nvIFR.nvIFROGLCreateTransferToHwEncObject(m_hSession, &config,
		&m_hTransferObject) != NV_IFROGL_SUCCESS)
	{
		snprintf(errStrTmp, 256,
			"Failed to create a NvIFROGL transfer object. w=%d, h=%d\n",
			config.width, config.height);
		THROW(errStrTmp);
	}
}

void GPUEncTrans::captureHwEnc()
{
	GLint drawFBO;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFBO);

	GLenum readBuffer;
	glGetIntegerv(GL_READ_BUFFER, (GLint *)&readBuffer);
	if (readBuffer == GL_BACK) readBuffer = GL_BACK_LEFT;
	else if (readBuffer == GL_FRONT) readBuffer = GL_FRONT_LEFT;

	int renderMode = 0;
	glGetIntegerv(GL_RENDER_MODE, &renderMode);
	if (renderMode != GL_RENDER && renderMode != 0)
	{
		if(!alreadyWarnedRenderMode && fconfig->verbose)
		{
			fprintf(stderr, "WARNING: One or more readbacks skipped because render mode != GL_RENDER.\n");
			alreadyWarnedRenderMode = true;
		}
		return;
	}

	uintptr_t dataSize;
	const void *data;

	if (XCapture::nvIFR.nvIFROGLTransferFramebufferToHwEnc(m_hTransferObject,
		NULL, drawFBO, readBuffer, GL_NONE) != NV_IFROGL_SUCCESS)
	{
		throw_nvifr_error("Failed to transfer data from the framebuffer.");
	}

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
}

void GPUEncTrans::captureSys()
{
	GLint drawFBO;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFBO);

	GLenum readBuffer;
	glGetIntegerv(GL_READ_BUFFER, (GLint *)&readBuffer);
	if (readBuffer == GL_BACK) readBuffer = GL_BACK_LEFT;
	else if (readBuffer == GL_FRONT) readBuffer = GL_FRONT_LEFT;

	int renderMode = 0;
	glGetIntegerv(GL_RENDER_MODE, &renderMode);
	if (renderMode != GL_RENDER && renderMode != 0)
	{
		if(!alreadyWarnedRenderMode && fconfig->verbose)
		{
			fprintf(stderr, "WARNING: One or more readbacks skipped because render mode != GL_RENDER.\n");
			alreadyWarnedRenderMode = true;
		}
		return;
	}

	uintptr_t dataSize;
	const void *data;

	if (XCapture::nvIFR.nvIFROGLTransferFramebufferToSys(m_hSysTransferObject,
		drawFBO, readBuffer, NV_IFROGL_TRANSFER_FRAMEBUFFER_FLAG_NONE, 0, 0, 0,
		0) != NV_IFROGL_SUCCESS)
	{
		throw_nvifr_error("Failed to transfer data from the framebuffer.");
	}

	// lock the transferred data
	if (XCapture::nvIFR.nvIFROGLLockTransferData(m_hSysTransferObject, &dataSize,
		&data) != NV_IFROGL_SUCCESS)
	{
		THROW("Failed to lock the transferred data.");
	}

	// release the data buffer
	if (XCapture::nvIFR.nvIFROGLReleaseTransferData(m_hSysTransferObject) !=
		NV_IFROGL_SUCCESS)
	{
		THROW("Failed to release the transferred data.");
	}
}

extern "C"
{

	void *RRTransInit(Display *dpy, Window win, FakerConfig *fconfig)
	{
		_vgl_disableFaker();

		void *handle = NULL;
		try
		{
			handle = (void *)(new GPUEncTrans(dpy, win, fconfig));
		}
		catch(Error &e)
		{
			snprintf(errStr, MAXSTR + 14, "Error in %s -- %s", e.getMethod(),
				e.getMessage());
			handle = NULL;
		}

		_vgl_enableFaker();

		return handle;
	}

	int RRTransConnect(void *handle, char *receiverName, int port)
	{
		return 0;
	}

	RRFrame *RRTransGetFrame(void *handle, int width, int height, int format,
		int stereo)
	{
		_vgl_disableFaker();

		RRFrame *frame = NULL;
		try
		{
			GPUEncTrans *trans = (GPUEncTrans *)handle;
			if(!trans) THROW("Invalid handle");
			frame = trans->getFrame(width, height, format);
		}
		catch(Error &e)
		{
			snprintf(errStr, MAXSTR + 14, "Error in %s -- %s", e.getMethod(),
				e.getMessage());
			frame = NULL;
		}

		_vgl_enableFaker();

		return frame;
	}

	int RRTransReady(void *handle)
	{
		return 1;
	}

	int RRTransSynchronize(void *handle)
	{
		return 0;
	}

	int RRTransSendFrame(void *handle, RRFrame *frame, int sync)
	{
		_vgl_disableFaker();

		int ret = 0;
		try
		{
			GPUEncTrans *trans = (GPUEncTrans *)handle;
			if(!trans) THROW("Invalid handle");
			// A session is required. The session is associated with the current
			// OpenGL context.
			// sleep(10);
			// trans->setupNVIFRSYS();
			// trans->captureSys();
			trans->setupNVIFRHwEnc(frame->w, frame->h);
			trans->captureHwEnc();
		}
		catch(Error &e)
		{
			snprintf(errStr, MAXSTR + 14, "Error in %s -- %s", e.getMethod(),
				e.getMessage());
			ret = -1;
		}

		_vgl_enableFaker();

		return ret;
	}

	int RRTransDestroy(void *handle)
	{
		_vgl_disableFaker();

		int ret = 0;
		try
		{
			GPUEncTrans *trans = (GPUEncTrans *)handle;
			if(!trans) THROW("Invalid handle");
			delete trans;
		}
		catch(Error &e)
		{
			snprintf(errStr, MAXSTR + 14, "Error in %s -- %s", e.getMethod(),
				e.getMessage());
			ret = -1;
		}

		_vgl_enableFaker();

		return ret;
	}

	const char *RRTransGetError(void)
	{
		return errStr;
	}

} // extern "C"
