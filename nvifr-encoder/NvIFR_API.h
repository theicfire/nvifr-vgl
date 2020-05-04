/*!
 * \file
 * A helper class to access the NvIFR API.
 *
 * \copyright    
 * Copyright 2013-2015 NVIDIA Corporation.  All rights reserved.    
 * \n\n    
 * Please refer to the NVIDIA end user license agreement (EULA) associated    
 * with this source code for terms and conditions that govern your use of    
 * this software. Any use, reproduction, disclosure, or distribution of    
 * this software and related documentation outside the terms of the EULA    
 * is strictly prohibited. 
 */

#ifndef NVIFR_API_H
#define NVIFR_API_H

#include <string.h>
#include "NvIFROpenGL.h"

/*!
 * This class can be used to access the NvIFR API. To use this define a
 * variable e.g. 'static NvIFRAPI nvIFR;' call 'nvIFR.initialize()' and
 * call the API entry points through this class, e.g. 'nvIFR.nvIFROGLCreateSession(...)'.
 */

//! NvIFROpenGL API version number
#define ENCODEAPI_MAJOR(nvIFR) ((nvIFR.nvIFRLibVersion & 0xFF000000) >> 24)
#define ENCODEAPI_MINOR(nvIFR) ((nvIFR.nvIFRLibVersion & 0xFF0000) >> 16)

class NvIFRAPI
{
public:
    //! entry points
    PNVIFROGLCREATESESSION nvIFROGLCreateSession;
    PNVIFROGLDESTROYSESSION nvIFROGLDestroySession;
    PNVIFROGLCREATETRANSFERTOSYSOBJECT nvIFROGLCreateTransferToSysObject;
    PNVIFROGLCREATETRANSFERTOHWENCOBJECT nvIFROGLCreateTransferToHwEncObject;
    PNVIFROGLDESTROYTRANSFEROBJECT nvIFROGLDestroyTransferObject;
    PNVIFROGLTRANSFERFRAMEBUFFERTOSYS nvIFROGLTransferFramebufferToSys;
    PNVIFROGLTRANSFERFRAMEBUFFERTOHWENC nvIFROGLTransferFramebufferToHwEnc;
    PNVIFROGLLOCKTRANSFERDATA nvIFROGLLockTransferData;
    PNVIFROGLRELEASETRANSFERDATA nvIFROGLReleaseTransferData;
    PNVIFROGLGETERROR nvIFROGLGetError;
    PNVIFROGLGETHWENCCAPS nvIFROGLGetHwEncCaps;
    PNVIFROGLDEBUGMESSAGECALLBACK nvIFROGLDebugMessageCallback;
    unsigned int nvIFRLibVersion;

    /*!
     * Constructor.
     */
    NvIFRAPI() :
        nvIFROGLCreateSession(NULL),
        nvIFROGLDestroySession(NULL),
        nvIFROGLCreateTransferToSysObject(NULL),
        nvIFROGLCreateTransferToHwEncObject(NULL),
        nvIFROGLDestroyTransferObject(NULL),
        nvIFROGLTransferFramebufferToSys(NULL),
        nvIFROGLTransferFramebufferToHwEnc(NULL),
        nvIFROGLLockTransferData(NULL),
        nvIFROGLReleaseTransferData(NULL),
        nvIFROGLGetError(NULL),
        nvIFROGLGetHwEncCaps(NULL),
        nvIFROGLDebugMessageCallback(NULL)
    {
    }

    /*!
     * Initialize the NvIFR API entry points
     *
     * /return false if failed
     */
    bool initialize()
    {
        NV_IFROGL_API_FUNCTION_LIST apiFunctionList;

        memset(&apiFunctionList, 0, sizeof(apiFunctionList));
        apiFunctionList.version = NVIFROGL_VERSION;
        if (NvIFROGLCreateInstance(&apiFunctionList) != NV_IFROGL_SUCCESS) {
            return false;
        }

        nvIFROGLCreateSession = apiFunctionList.nvIFROGLCreateSession;
        nvIFROGLDestroySession = apiFunctionList.nvIFROGLDestroySession;
        nvIFROGLCreateTransferToSysObject = apiFunctionList.nvIFROGLCreateTransferToSysObject;
        nvIFROGLCreateTransferToHwEncObject = apiFunctionList.nvIFROGLCreateTransferToHwEncObject;
        nvIFROGLDestroyTransferObject = apiFunctionList.nvIFROGLDestroyTransferObject;
        nvIFROGLTransferFramebufferToSys = apiFunctionList.nvIFROGLTransferFramebufferToSys;
        nvIFROGLTransferFramebufferToHwEnc = apiFunctionList.nvIFROGLTransferFramebufferToHwEnc;
        nvIFROGLLockTransferData = apiFunctionList.nvIFROGLLockTransferData;
        nvIFROGLReleaseTransferData = apiFunctionList.nvIFROGLReleaseTransferData;
        nvIFROGLGetError = apiFunctionList.nvIFROGLGetError;
        nvIFROGLGetHwEncCaps = apiFunctionList.nvIFROGLGetHwEncCaps;
        nvIFROGLDebugMessageCallback = apiFunctionList.nvIFROGLDebugMessageCallback;
        nvIFRLibVersion=apiFunctionList.nvIFRLibVersion;
        return true;
    }
};

#endif // NVIFR_API_H

