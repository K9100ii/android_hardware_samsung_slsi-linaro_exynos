/*
 * Copyright (c) 2012-2014 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 */

#define LOG_TAG "VXOSAL"
#include <cutils/log.h>

#include <dlfcn.h>

#include <VX/vx.h>
#include <VX/vx_internal.h>

#include "ExynosVisionCommonConfig.h"

#include "vx_osal.h"

vx_module_handle_t vxLoadModule(vx_char *name)
{
    vx_module_handle_t mod;

    mod = dlopen(name, RTLD_NOW|RTLD_LOCAL);
    if (mod == 0)
        VXLOGE("%s", dlerror());

    return mod;
}

void vxUnloadModule(vx_module_handle_t mod)
{
    if (dlclose(mod) != 0)
        VXLOGE("%s", dlerror());
}

vx_symbol_t vxGetSymbol(vx_module_handle_t mod, vx_char *name)
{
    vx_symbol_t sym;

    sym = (vx_symbol_t)dlsym(mod, name);
    if (sym == 0)
        VXLOGW("%s", dlerror());

    return sym;
}

/******************************************************************************/
// EXTERNAL API (NO COMMENTS HERE, SEE HEADER FILES)
/******************************************************************************/

