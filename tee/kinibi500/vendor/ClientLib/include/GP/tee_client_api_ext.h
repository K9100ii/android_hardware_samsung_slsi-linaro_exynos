/*
 * Copyright (c) 2016-2017 TRUSTONIC LIMITED
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the TRUSTONIC LIMITED nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef   __TEE_CLIENT_API_EXT_H__
#define   __TEE_CLIENT_API_EXT_H__

#pragma GCC visibility push(default)

/*
 * Registers two contexts which are platform-specific, such the virtual machine
 * and the application in an Android-based environment.
 */
TEEC_EXPORT void TEEC_TT_RegisterPlatformContext(
    void                *globalContext,
    void                *localContext);

#pragma GCC visibility pop

typedef TEEC_Result (*TEEC_TT_Callback_t)(uint32_t          commandId,
                                          uint32_t          paramTypes,
                                          TEEC_Parameter    params[2]);

#pragma GCC visibility push(default)

TEEC_EXPORT TEEC_Result TEEC_TT_RegisterCallback(
    TEEC_TT_Callback_t callback);

TEEC_EXPORT void TEEC_TT_UnregisterCallback(void);

#pragma GCC visibility pop

#define TEEC_TT_PARAM_TYPE_GET(types, entry) \
    (((types) >> (entry * 4)) & 0xf)

/* Change param type into its opposite direction
 * IN => OUT
 * OUT => IN
 * INOUT => INOUT
 * Warning: change values 9 and 0xa too (now RFU in GP) */
#define TEEC_TT_PARAM_TYPE_OPPOSITE(entry) \
    ((entry & (~3)) + ((entry & 1) << 1) + ((entry & 2) >> 1))

#define CALLBACK_BUFFER_DEFAULT_SIZE 1024

/**
 * Callback return codes mask: 0xFFFE30xx
 * Assuming callback param 3 is TEE_PARAM_TYPE_VALUE_INOUT (3)
 * and callback param 2 is TEE_PARAM_TYPE_NONE (0)
 */
#define TEEC_TT_CALLBACK_MASK           ((TEEC_Result)0xFFFE3000)

/**
 * Callback return mask
 * Isolating user-defined params 0 and 1
 */
#define TEEC_TT_CALLBACK_MASK_PARAMS    ((TEEC_Result)0xFFFFFF00)


/**
 * Callback command ID for CA to TA comunication.
 * Get parameters types for callback. Will be followed by _CONTINUE or _SET_OUTPUT.
 */
#define TEEC_TT_CALLBACK_CMD_GET_INPUT  1001

/**
 * Callback command ID for CA to TA comunication.
 * Return response from callback. Will be followed by _CONTINUE.
 */
#define TEEC_TT_CALLBACK_CMD_SET_OUTPUT 1002

/**
 * Callback command ID for CA to TA comunication.
 * Continue suspended operation. params must be as for original operation.
 */
#define TEEC_TT_CALLBACK_CMD_CONTINUE   1003

/**
 * Callback command ID for CA to TA comunication.
 * Continue suspended operation. params must be as for original operation.
 */
#define TEEC_TT_CALLBACK_CMD_ABORT      1004


/**
 * Dynamically activates the LibTee logging mechanism
 */
typedef enum {
    TEEC_TRACE,
    TEEC_DEBUG,
    TEEC_INFO,
    TEEC_WARNING,
    TEEC_ERROR,
    TEEC_NO_LOG,
} TEEC_LogLevel;

#pragma GCC visibility push(default)

TEEC_EXPORT void TEEC_TT_SetLogLevel(TEEC_LogLevel level);

#pragma GCC visibility pop

#endif /* __TEE_CLIENT_API_EXT_H__ */
