/*
 * Copyright (c) 2016-2020 TRUSTONIC LIMITED
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

#ifndef ISERVICE_H
#define ISERVICE_H

#include <sys/mman.h>

#include "MobiCoreDriverApi.h"

class Tci {
    size_t length_ = 0;
    void* address_ = nullptr;
public:
    Tci(size_t length): length_(length) {}

    ~Tci() {
        free();
    }

    mcResult_t alloc() {
        mcResult_t ret = MC_DRV_OK;
#if defined(__QNX__)
        uint8_t* addr;
        ret = mcMallocWsm(MC_DEVICE_ID_DEFAULT, 0, length_, &addr, 0);
#else
        void* addr;
        addr = ::mmap(NULL, length_, PROT_READ | PROT_WRITE,
                      MAP_ANONYMOUS | MAP_SHARED, 0, 0);
        if (addr == MAP_FAILED) {
            ret = MC_DRV_ERR_NO_FREE_MEMORY;
        }
#endif
        if (ret == MC_DRV_OK) {
            address_ = addr;
        }

        return ret;
    }

    void free() {
        if (address_) {
#if defined(__QNX__)
            uint8_t* addr = static_cast<uint8_t*>(address_);
            (void)mcFreeWsm(MC_DEVICE_ID_DEFAULT, addr);
#else
            ::munmap(address_, length_);
#endif
            address_ = nullptr;
        }
    }

    uint8_t* address() const {
        return static_cast<uint8_t*>(address_);
    }

    size_t length() const {
        return length_;
    }
};

class IService {
public:
    virtual ~IService() {}
    virtual const char* displayName() const = 0;
    virtual int open() = 0;
    virtual void close() = 0;
    // Use signum = 0 for normal exit
    virtual void receiveSignal(int /*signum*/) {}
};

#endif /* ISERVICE_H */
