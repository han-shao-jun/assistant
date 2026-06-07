#ifndef STM32_ISP_H
#define STM32_ISP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#define STM32_ACK  0x79u
#define STM32_NACK 0x1Fu

typedef enum STM32_CMD
{
    Sync = 0x7f,
    Get = 0x00,
    Get_Version = 0x01,
    Get_PID = 0x02,
    Read_Memory = 0x11,
    Go = 0x21,
    Write_Memory = 0x31,
    Erase = 0x43,
    Write_Protect = 0x63,
    Write_Unprotect = 0x73,
    Readout_Protect = 0x82,
    Readout_Unprotect = 0x92
} STM32_CMD_t;

bool ISP_Sync();
bool ISP_GetVersion(uint8_t *rec);
bool ISP_GetPID(uint8_t *rec);
bool ISP_Go(uint32_t addr);
bool ISP_Erase();
bool ISP_ReadFlash(uint32_t addr, uint8_t *data, uint32_t len);
bool ISP_ProgramFlash(uint32_t addr, uint8_t *data, uint32_t len);

#ifdef __cplusplus
};
#endif

#endif // STM32_ISP_H
