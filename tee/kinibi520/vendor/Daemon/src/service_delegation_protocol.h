/*
 * Copyright (c) 2013-2020 TRUSTONIC LIMITED
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
#ifndef SERVICE_DELEGATION_PROTOCOL_H
#define SERVICE_DELEGATION_PROTOCOL_H


/** List of instructions to be executed by the daemon. */

/** Instruction to create a partition */
#define DELEGATION_INSTRUCTION_PARTITION_CREATE     0x01
/** Instruction to open a partition */
#define DELEGATION_INSTRUCTION_PARTITION_OPEN       0x02
/** Instruction to read a sector in a partition */
#define DELEGATION_INSTRUCTION_PARTITION_READ       0x03
/** Instruction to write a sector in a partition */
#define DELEGATION_INSTRUCTION_PARTITION_WRITE      0x04
/** Instruction to set the size of a partition */
#define DELEGATION_INSTRUCTION_PARTITION_SET_SIZE   0x05
/** Instruction to flush a partition to the physical storage */
#define DELEGATION_INSTRUCTION_PARTITION_SYNC       0x06
/** Instruction to close a partition */
#define DELEGATION_INSTRUCTION_PARTITION_CLOSE      0x07

/**
 * @note in the following structures, nInstructionID encodes the partition
 * identifier (nPartitionID)  in the high-nibble and the instruction
 * DELEGATION_INSTRUCTION_PARTITION_XXX in the low-nibble */


typedef struct {
    uint32_t nInstructionID; /**< nPartitionID | DELEGATION_INSTRUCTION_PARTITION_CREATE,
                                 DELEGATION_INSTRUCTION_PARTITION_OPEN, DELEGATION_INSTRUCTION_PARTITION_SYNC or
                                 DELEGATION_INSTRUCTION_PARTITION_CLOSE */
} DELEGATION_GENERIC_INSTRUCTION;

typedef struct {
    uint32_t nInstructionID; /**< nPartitionID | DELEGATION_INSTRUCTION_PARTITION_READ
                                 or DELEGATION_INSTRUCTION_PARTITION_WRITE */
    uint32_t nSectorID; /**< Sector identifier */
    uint32_t nWorkspaceOffset; /**< Offset in the workspace from where to read or write the sector content */
} DELEGATION_RW_INSTRUCTION;

typedef struct {
    uint32_t nInstructionID; /**< nPartitionID | DELEGATION_INSTRUCTION_PARTITION_SET_SIZE */
    uint32_t nNewSize; /**< New size for the partition */
} DELEGATION_SET_SIZE_INSTRUCTION;

/**
 * This union defines the different types of instructions that can be sent to
 * the daemon.
 */
typedef union {
    DELEGATION_GENERIC_INSTRUCTION    sGeneric;
    DELEGATION_RW_INSTRUCTION         sReadWrite;
    DELEGATION_SET_SIZE_INSTRUCTION   sSetSize;
} DELEGATION_INSTRUCTION;

/**
 * This structure is used by the daemon to report administrative data such as
 * the number of flushes executed, the size of the partition or if an error
 * happened during the execution of the instructions.
 */
typedef struct {
    uint32_t    nSyncExecuted;
    uint32_t    nPartitionErrorStates[16];
    uint32_t    nPartitionOpenSizes[16];
} DELEGATION_ADMINISTRATIVE_DATA;

#endif /* SERVICE_DELEGATION_PROTOCOL_H */
/**
 * @}
 */
