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

#ifndef GP_SHARED_MEM_H
#define GP_SHARED_MEM_H

#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>

#include <sys/mman.h>

#include "tee_client_api.h"

namespace trustonic {

// Keep internal track of allocated shared memories
class AllocatedSharedMemories {
    class MmapBuffer {
        void* address_ = nullptr;
        size_t size_ = 0;
        size_t allocated_size_ = 0;
    public:
        MmapBuffer(size_t size): size_(size) {}

        ~MmapBuffer() {
            if (!address_) {
                return;
            }

            ::munmap(address_, allocated_size_);
        }

        bool create() {
            // mmap cannot allocate 0-size buffers (unlike malloc), but we do
            // want a valid buffer/address
            allocated_size_ = size_ ? size_ : 1;
            void* address = ::mmap(0, allocated_size_, PROT_READ | PROT_WRITE,
                                   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
            if (address == MAP_FAILED) {
                return false;
            }

            address_ = address;
            return true;
        }

        void* address() const {
            return address_;
        }
    };

    std::vector<std::unique_ptr<MmapBuffer>> buffers_;
    std::mutex buffers_mutex_;
public:
    void* allocate(size_t size) {
        std::unique_ptr<MmapBuffer> buffer(new MmapBuffer(size));
        if (!buffer->create()) {
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(buffers_mutex_);
        buffers_.push_back(std::move(buffer));
        LOG_D("allocated shared mem, size %zu", buffers_.size());
        return buffers_.back()->address();
    }

    bool free(void *address) {
        std::lock_guard<std::mutex> lock(buffers_mutex_);
        auto it = std::find_if(buffers_.begin(), buffers_.end(),
        [address](const std::unique_ptr<MmapBuffer>& buffer) {
            return address == buffer->address();
        });
        if (it == buffers_.end()) {
            LOG_D("shared mem not found");
            return false;
        }

        buffers_.erase(it);
        LOG_D("freed shared mem, size %zu", buffers_.size());
        return true;
    }

    void free_all(void) {
        std::lock_guard<std::mutex> lock(buffers_mutex_);
        buffers_.clear();
        LOG_D("freed shared mem, size %zu", buffers_.size());
    }
};

}

#endif // GP_SHARED_MEM_H
