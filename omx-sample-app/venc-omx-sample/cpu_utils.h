/*--------------------------------------------------------------------------
Copyright (c) 2020, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--------------------------------------------------------------------------*/

/*============================================================================
                            O p e n M A X   w r a p p e r s
                             O p e n  M A X   C o r e

  This module contains the implementation of the OpenMAX core & component.

*//*========================================================================*/

//////////////////////////////////////////////////////////////////////////////
//                             Include Files
//////////////////////////////////////////////////////////////////////////////

#ifndef VENC_OMX_SAMPLE_CPU_UTILS_H_
#define VENC_OMX_SAMPLE_CPU_UTILS_H_

#include <sys/types.h>

#define VMRSS_LINE 15
#define PROCESS_ITEM 14

typedef struct TotalCpuOccupy {
  // Step by step from system start to current time, running time in user mode.
  unsigned int user;
  // Step by step from system start to current time, CPU time occupied by processes with nice values.
  unsigned int nice;
  // Step by step from system start to current time, Running time in the core state.
  unsigned int system;
  // Step by step from system start to current time, Waiting time other than IO waiting time iowait.
  unsigned int idle;
}TotalCpuOccupy_t;

typedef struct ProcessCpuOccupy {
  pid_t pid;
  unsigned int utime;   // The time the task runs in user mode (jiffies).
  unsigned int stime;   // The time the task is running in the core state (jiffies).
  unsigned int cutime;  // The time all dead threads are running in user mode (jiffies).
  unsigned int cstime;  // All dead time in core state (jiffies).
}ProcessCpuOccupy_t;

// Get occupied physical memory.
int GetPhysicalMem(const pid_t p);
// Get total system memory.
int GetTotalMem();
// Get the total CPU time.
unsigned long long GetCpuTotalOccupy();
// Get the CPU time of a process.
unsigned long long GetCpuProcessOccupy(const pid_t p);

float GetProcessCpu(pid_t p, unsigned long long time);  // Get process CPU usage.
float GetProcessMem(pid_t p);  // Get process memory usage.

#endif  // VENC_OMX_SAMPLE_CPU_UTILS_H_
