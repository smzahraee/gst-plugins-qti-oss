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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "cpu_utils.h"

int GetPhysicalMem(const pid_t p) {
  char file[64] = {0};  // filename

  FILE *fd;  // Define file pointer fd
  char line_buff[256] = {0};  // Read line buffer
  snprintf(file, sizeof(file), "/proc/%d/status", (int)p);  // Line 11 in the file contains

  fprintf(stderr, "current pid:%d\n", p);
  fd = fopen(file, "r");  // Open the file in R read mode and assign it to the pointer fd
  if (fd == NULL) {
    fprintf(stderr, "file open failed.");
    return 0;
  }

  // Get vmrss: actual physical memory usage
  int i;
  char name[32];  // Store project name
  int vmrss;  // Store peak memory size
  for (i=0; i < VMRSS_LINE-1; i++) {
    char* line = fgets(line_buff, sizeof(line_buff), fd);
  }  // Read line 15

  if (fgets(line_buff, sizeof(line_buff), fd) == NULL) {  // Read the data in the VmRSS line, VmRSS is on line 15
    return 0;
  }
  sscanf(line_buff, "%s %d", name, &vmrss);
  fprintf(stderr, "====%s:%d====\n", name, vmrss);
  fclose(fd);     // Close file fd
  return vmrss;
}

int GetTotalMem() {
  const char* file = "/proc/meminfo";  // file name

  FILE *fd; // Define file pointer fd
  char line_buff[256] = {0};  // Read line buffer
  fd = fopen(file, "r");  // Open the file in R read mode and assign it to the pointer fd

  if (fd == NULL) {
    fprintf(stderr, "file open failed.");
    return 0;
  }

  // Get memtotal: total memory footprint
  int i;
  char name[32];  // Store project name
  int memtotal;  // Store peak memory size
  if (fgets(line_buff, sizeof(line_buff), fd) == NULL) {  // Read the data of the memtotal line, memtotal is in the first line
    return 0;
  }
  sscanf(line_buff, "%s %d", name,&memtotal);
  fprintf(stderr, "====%s:%d====\n", name,memtotal);
  fclose(fd);  // Close file fd
  return memtotal;
}

float GetProcessMem(pid_t p) {
  int phy = GetPhysicalMem(p);
  int total = GetTotalMem();
  float occupy = 100.0*(phy*1.0)/(total*1.0);
  fprintf(stderr, "====process mem occupy:%.6f%====\n", occupy);
  return occupy;
}

unsigned long long GetCpuProcessOccupy(const pid_t p) {
  char file[64] = {0};  // filename
  ProcessCpuOccupy_t t;

  FILE *fd;  // Define file pointer fd
  char line_buff[1024] = {0};  // Read line buffer
  snprintf(file, sizeof(file), "/proc/%d/stat", (int)p);  // Line 11 in the file contains

  fprintf (stderr, "current pid:%d\n", p);
  fd = fopen(file, "r");  // Open the file in R read mode and assign it to the pointer fd
  // Read the string of length buff from the fd file and store it in the space with the starting address of buff
  if (fd == NULL) {
    fprintf(stderr, "file open failed.");
    return 0;
  }

  if (fgets(line_buff, sizeof(line_buff), fd) == NULL) {
    return 0;
  }

  sscanf(line_buff,"%u", &t.pid);  // Get the first item
  const char* q = line_buff;  // Get the starting pointer from item 14
    if (PROCESS_ITEM > 1) {
      int i;
      int count = 0;
      for (i = 0; i < strlen(line_buff); i++) {
        if (' ' == *q) {
          count++;
          if (count == PROCESS_ITEM - 1) {
            q++;
            break;
          }
        }
        q++;
      }
  }

  sscanf(q,"%u %u %u %u", &t.utime, &t.stime, &t.cutime, &t.cstime);  // Format items 14, 15, 16, 17

  fprintf(stderr, "====pid %u: %u %u %u %u====\n", t.pid, t.utime, t.stime, t.cutime, t.cstime);
  fclose(fd);  // Close file fd
  return ((unsigned long long)t.utime + (unsigned long long)t.stime +
          (unsigned long long)t.cutime + (unsigned long long)t.cstime);
}

unsigned long long GetCpuTotalOccupy() {
  FILE *fd;  // Define file pointer fd
  char buff[1024] = {0};
  TotalCpuOccupy_t t;

  fd = fopen("/proc/stat", "r");  // Open the file in R read mode and assign it to the pointer fd
  if (fd == NULL) {
    fprintf(stderr, "file open failed.");
    return 0;
  }
  // Read the string of length buff from the fd file and store it in the space with the starting address of buff
  if (fgets(buff, sizeof(buff), fd) == NULL) {
    return 0;
  }
  // The following is the result of converting the buff string into data according
  // to the parameter format and storing it into the corresponding structure parameter.
  char name[16];  // Temporarily used to store strings
  sscanf (buff, "%s %u %u %u %u", name, &t.user, &t.nice,&t.system, &t.idle);

  fprintf (stderr, "====%s: %u %u %u %u====\n", name, t.user, t.nice, t.system, t.idle);
  fclose(fd);  // Close file fd
  return ((unsigned long long)t.user + (unsigned long long)t.nice +
          (unsigned long long)t.system + (unsigned long long)t.idle);
}

float GetProcessCpu(pid_t p, unsigned long long time) {
  unsigned long long procputime;
  procputime = GetCpuProcessOccupy(p);
  float pcpu = 100.0 * procputime / (GetCpuTotalOccupy() - time);
  fprintf(stderr, "====pcpu:%.6f%====\n", pcpu);
  return pcpu;
}

