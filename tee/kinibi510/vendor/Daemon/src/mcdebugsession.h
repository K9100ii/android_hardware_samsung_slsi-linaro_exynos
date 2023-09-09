/*
 * Copyright (c) 2013-2018 TRUSTONIC LIMITED
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

#ifndef MC_DEBUGSESSION_H_
#define MC_DEBUGSESSION_H_

#define DEBUGSESSION_VERSION_MAJOR 1
#define DEBUGSESSION_VERSION_MINOR 2

#define MAX_THREADS         200
#define MAX_CORES           8
#define MAX_FC_EVENTS       64
#define MAX_SESSIONS        128
#define MAX_L2_TABLES       124
#define MAX_EXC_LOG         5
#define MAX_TIMESTAMPS      100
#define MAX_CRASH           10

#define MC_UUID_DEBUG_SESSION { { 0x08, 0x05, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }
#ifdef ANDROID
#define DEBUG_SESSION_DUMP_FILE "/data/local/tmp/debugsession"
#else
#define DEBUG_SESSION_DUMP_FILE "/tmp/debugsession"
#endif

/*
 * Trustonic TEE DebugSession interface
 */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t no_entries;
    struct {
        uint32_t type;
        uint32_t version;
        uint32_t offset;
        uint32_t size;
    } headers[3];
} debugsession_header_t;

typedef struct {
    char productid[128];
    char productid_porting[128];
} tee_identity_t;

typedef struct {
    uint32_t threadid;
    uint32_t ipc_partner;
    uint32_t flags_priority;
    uint32_t last_syscall;
    uint64_t pc;
    uint64_t sp;
    uint64_t last_modified_time;
    uint64_t total_user_time;
    uint64_t total_sys_time;
} tee_thread_t;

#define MAX_TRACED_THREAD   32
// has to be a power of 2, since we use that for optimization purpose
#define SYS_EV_PER_TH       512

#define TH_SYS_EV_ENTER  1 // enter to kernel
#define TH_SYS_EV_EXIT   2 // exit from kernel
#define TH_SYS_EV_RETRY  3 // restart ipc from kernel

typedef struct {
    uint32_t timestamp;
    uint8_t syscall_id;
    // enter, exit, retry
    uint8_t type;
    uint16_t core_id;

    uint32_t ipc_partner;
    uint32_t flags_priority;
    uint64_t pc;

    // syscall interface use 32b registers
    uint32_t param[5];
} sys_event_t;

typedef struct {
    uint32_t threadid;
    sys_event_t event[SYS_EV_PER_TH];
} tee_sys_event_t;

typedef struct {
    uint64_t r0;
    uint64_t r1;
    uint64_t r2;
    uint64_t r3;
} tee_fastcall_params_t;

typedef struct {
        uint64_t entryTimestamp;
        uint64_t exitTimestamp;
        uint64_t cycles;
        uint32_t entryMode;
        uint32_t exitMode;
        uint32_t currentThreadId;
        tee_fastcall_params_t fc_params_entry;
        tee_fastcall_params_t fc_params_exit;
} tee_timestamp_t;

typedef struct {
    tee_timestamp_t timestamps[MAX_TIMESTAMPS];
    uint32_t currentTimestampIdx;
} tee_entry_exit_t;

typedef struct {
    uint32_t max_threads;
    tee_thread_t threads[MAX_THREADS];
    tee_entry_exit_t entry_exit_timestamps[MAX_CORES];
    uint32_t frequencyKHz;
    struct {
        uint32_t threadid;
        uint32_t ip;
        uint32_t sp;
        uint32_t coreId;
        uint32_t taskOffset;
        uint32_t mcLibOffset;
    } crash[MAX_CRASH];
    uint32_t currentCrashIdx;
    uint32_t totalCrashes;
    tee_sys_event_t thread_event[MAX_TRACED_THREAD];
} tee_kernel_t;

typedef struct {
    uint32_t num_tables;
    uint32_t total_pages;
    uint32_t rfu_padding[2];
    struct {
        uint16_t        cntBlobRX;
        uint16_t        cntBlobRW;
        uint16_t        cntClient;
        uint16_t        cntWsm;

        uint16_t        cntPhys;
        uint16_t        cntHeap;
        uint16_t        cntMisc;
        uint16_t        cntGuard;
    } area_counters;
} task_memory_t;

typedef struct {
    uint32_t sessionId; // valid
    uint32_t serviceType;
    uint32_t driverId;
    uint32_t sessionState;

    uint32_t isGP;
    uint32_t rfu_padding[3];

    uint8_t  uuid[16];

    uint32_t spid;
    uint32_t rootid;
    uint32_t maxThreads;
    uint32_t numInstances;

    uint32_t serviceFlags;
    uint32_t referencedL2Tables;
    uint32_t referencedPages;
    uint32_t mainThread;

    uint32_t secondThread; // notificationHandler?
    uint32_t rfu_padding1[3];

    task_memory_t task_memory;
    struct {
        uint32_t msgid;
        uint32_t msgLenPages;
        uint32_t mr0;
        uint32_t mr2;
    } last_ipch;
} tee_rtm_session_t;

typedef struct {
    uint32_t receiver;
    uint32_t trapType;
    uint32_t trapMeta;
    uint32_t rfu_padding;
} lastExc_t;

typedef struct {
    uint32_t write_pos;
    uint32_t rfu_padding[3];

    lastExc_t lastExc[MAX_EXC_LOG];
} exch_t;

typedef struct {
    uint32_t heapUsage;
    uint32_t heapSize;
    uint32_t pending_free_counter;
    uint32_t l2TablePoolUsage;
    uint32_t l2TablePoolMax;
    uint32_t pTablePoolUsage;
    uint32_t pTablePoolMax;
} mem_usage_t;

typedef struct { //type TRTM
    mem_usage_t mem_usage;
    struct {
        uint32_t lastReceiver; //threadids
        uint32_t lastSender;
        uint32_t lastError;
        uint32_t rfu_padding;
    } ipch;
    exch_t exch;
    struct {
        uint32_t lastMcpCommand;
        uint32_t lastOpenedSession;
        uint32_t lastClosedSession;
        uint32_t lastError;
    } msh;
    struct {
        uint32_t sId;
        uint32_t notification;
    } siqh;
    struct { //TODO was empty struct, just padding to compile correctly
        uint32_t rfu_padding[4];
    } pm_inth;
    uint32_t max_sessions;
    uint32_t rfu_padding;
    tee_rtm_session_t sessions[MAX_SESSIONS];
} tee_rtm_t;

#endif /* MC_DEBUGSESSION_H_ */
