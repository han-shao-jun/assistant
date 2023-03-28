#ifndef YMODEM_H
#define YMODEM_H


#include <cstdint>
#include <QByteArray>
#include <QString>
#include <QDebug>

#define SOH                     (0x01)  /* start of 128-byte data packet */
#define STX                     (0x02)  /* start of 1024-byte data packet */
#define EOT                     (0x04)  /* end of transmission */
#define ACK                     (0x06)  /* acknowledge */
#define NAK                     (0x15)  /* negative acknowledge */
#define CA                      (0x18)  /* two of these in succession aborts transfer */
#define CRC16                   (0x43)  /* 'C' == 0x43, request 16-bit CRC */

#define ABORT1                  (0x41)  /* 'A' == 0x41, abort by user */
#define ABORT2                  (0x61)  /* 'a' == 0x61, abort by user */

#define PACKET_SEQNO_INDEX      (1)
#define PACKET_SEQNO_COMP_INDEX (2)

#define PACKET_HEADER           (3)
#define PACKET_TRAILER          (2)
#define PACKET_OVERHEAD         (PACKET_HEADER + PACKET_TRAILER)
#define PACKET_SIZE             (128)
#define PACKET_1K_SIZE          (1024)

#define FILE_NAME_LENGTH        (256)
#define FILE_SIZE_LENGTH        (16)

#define NAK_TIMEOUT             (0x100000)
#define MAX_ERRORS              (5)

class Ymodem
{
public:
    Ymodem();

    virtual ~Ymodem();
    static void PrepareIntialPacket(QByteArray& data, const char * fileName, uint32_t length);
    static void PrepareDataPacket(QByteArray& SourceBuf, QByteArray& data, uint8_t pktNo, uint32_t sizeBlk);
    static void PrepareEndPacket(QByteArray& data);

private:
    static void Int2Str(uint8_t *str, uint32_t intnum);
    static uint16_t UpdateCRC16(uint16_t crcIn, uint8_t byte);
    static uint16_t Cal_CRC16(const uint8_t *data, uint32_t size);
    static uint8_t CalChecksum(const uint8_t *data, uint32_t size);
};


#endif //YMODEM_H
