/* Copyright stsp, FDPP project */

#ifndef BPRM_H
#define BPRM_H

#ifndef __ASSEMBLER__

#include <stdint.h>

struct _bprm {
  uint16_t BprmLen;
  uint16_t InitEnvSeg;          /* initial env seg                      */
  uint8_t ShellDrive;           /* drive num to start shell from        */
  uint8_t DeviceDrive;          /* drive num to load DEVICE= from       */
  uint8_t CfgDrive;             /* drive num to load fdppconf.sys from  */
  uint32_t DriveMask;           /* letters of masked drive are skipped  */
  uint16_t HeapSize;            /* heap for initial allocations         */
  uint16_t HeapSeg;             /* heap segment or 0 if after kernel    */
#define FDPP_FL_ALT_LAYOUT  1   /* alter heap layout in some weird ways */
#define FDPP_FL_ALT_LAYOUT2 2   /* alter heap layout in some weird ways */
  uint16_t Flags;
};

#endif

#define BPRM_VER 5
#define BPRM_MIN_VER 4

#define FDPP_BS_SEG 0x1fe0
#define FDPP_BS_OFF 0x7c00

#define FDPP_PLT_OFFSET 0x100
#define FDPP_BPRM_VER_OFFSET 0x104
#define FDPP_BPRM_OFFSET 0x106

#endif
