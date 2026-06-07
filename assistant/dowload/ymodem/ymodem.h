#ifndef YMODEM_H
#define YMODEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define SOH                     (0x01u) /* start of 128-byte data packet */
#define STX                     (0x02u) /* start of 1024-byte data packet */
#define EOT                     (0x04u) /* end of transmission */
#define ACK                     (0x06u) /* acknowledge */
#define NAK                     (0x15u) /* negative acknowledge */
#define CA                      (0x18u) /* two of these in succession aborts transfer */
#define CRC16                   (0x43u) /* 'C' == 0x43, request 16-bit CRC */

#define ABORT1                  (0x41u) /* 'A' == 0x41, abort by user */
#define ABORT2                  (0x61u) /* 'a' == 0x61, abort by user */

#define PACKET_SEQNO_INDEX      (1)
#define PACKET_SEQNO_COMP_INDEX (2)

#define PACKET_HEADER           (3u)
#define PACKET_TRAILER          (2u)
#define PACKET_OVERHEAD         (PACKET_HEADER + PACKET_TRAILER)
#define PACKET_SIZE             (128u)
#define PACKET_1K_SIZE          (1024u)

#define FILE_NAME_LENGTH        (256u)
#define FILE_SIZE_LENGTH        (16u)

#define NAK_TIMEOUT             (0x100000u)
#define MAX_ERRORS              (5u)

#define IS_CAP_LETTER(c)        (((c) >= 'A') && ((c) <= 'F'))
#define IS_LC_LETTER(c)         (((c) >= 'a') && ((c) <= 'f'))
#define IS_09(c)                (((c) >= '0') && ((c) <= '9'))
#define ISVALIDHEX(c)           (IS_CAP_LETTER(c) || IS_LC_LETTER(c) || IS_09(c))
#define ISVALIDDEC(c)           IS_09(c)
#define CONVERTDEC(c)           (c - '0')

#define CONVERTHEX_ALPHA(c)     (IS_CAP_LETTER(c) ? ((c) - 'A' + 10) : ((c) - 'a' + 10))
#define CONVERTHEX(c)           (IS_09(c) ? ((c) - '0') : CONVERTHEX_ALPHA(c))

void Ymodem_Int2Str(uint32_t intNum, uint8_t *str);
bool Ymodem_Str2Int(const uint8_t *inputStr, uint32_t *intNum);
uint16_t Ymodem_Cal_CRC16(const uint8_t *data, uint32_t size);
uint8_t Ymodem_CalChecksum(const uint8_t *data, uint32_t size);
void Ymodem_PrepareIntialPacket(uint8_t *data, const char *fileName, uint32_t length);
void Ymodem_PrepareDataPacket(const uint8_t *sourceBuf, uint8_t *data, uint8_t pktNo,
                              uint32_t sizeBlk);
void Ymodem_PrepareEndPacket(uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif // YMODEM_H
