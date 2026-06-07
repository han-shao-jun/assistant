#ifndef CONVERT_H
#define CONVERT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdint.h>

#define HEX_MAX_LENGTH 521
#define HEX_MIN_LEN    11

typedef enum
{
    RES_OK = 0,              // 正确
    RES_DATA_TOO_LONG,       // 数据太长
    RES_DATA_TOO_SHORT,      // 数据太短
    RES_NO_COLON,            // 无标号
    RES_TYPE_ERROR,          // 类型出错，或许不存在
    RES_LENGTH_ERROR,        // 数据长度字段和内容的总长度不对应
    RES_CHECK_ERROR,         // 校验错误
    RES_HEX_FILE_NOT_EXIST,  // HEX文件不存在
    RES_BIN_FILE_PATH_ERROR, // BIN文件路径可能不正确
    RES_WRITE_ERROR,         // 写数据出错
    RES_HEX_FILE_NO_END,     // hex文件没有结束符
} RESULT_STATUS;

//: 冒号	数据每行都由冒号开头
typedef struct {
    uint8_t len;   // A:数据长度 1 Byte ，表示本行数据的长度
    uint16_t addr; // B:数据地址 2 Byte ，表示数据的起始地址
    uint8_t type;  // C:数据类型 1 Byte
    uint8_t *data; // D:具体数据 N Byte ，表示本行中数据字节的数量，它和A说明的数据长度一致
} BinFarmat;

typedef struct {
    const char *HexFileName;
    const char *BinFileName;
    uint32_t StartAddr;
    uint32_t EndAddr;
    uint32_t FileSize;
} FwFileInfo;

// E	校验和 1 Byte ，检验和 = 0x100 - 累加和

// 数据类型详解
// ‘00’	数据记录：用来记录数据，HEX文件的大部分记录都是数据记录
// ‘01’	文件结束记录：用来标识文件结束，放在文件的最后，标识HEX文件的结尾
// ‘02’	扩展段地址记录：用来标识扩展段地址的记录
// ‘03’	开始段地址记录：开始段地址记录
// ‘04’	扩展线性地址记录：用来标识扩展线性地址的记录
// ‘05’	开始线性地址记录：开始线性地址记录
// 具体数据分析如下（以keil生成hex文件为例）
// 1. Hex文件第一行
// :02 0000 04 0800 F2
// 02：代表本行有2个字节数据
// 0000：本行数据的起始地址（偏移地址）
// 04：扩展线性地址标识，表面后面2个字节数据是后面数据的基地址
// 注：由于每行标识数据地址的只有2Byte，所以最大只能到64K，为了可以保存高地址的数据，故有了扩展线性地址记录也叫作32位地址记录
// 或HEX386记录.这些记录含数据的高16位，扩展线性地址记录总是有两个数据字节。
// 0800：是扩展地址 (0x0800 << 16) = 0x08000000后面的数据记录都以这个地址为基地址。
// F2: 记录本行校验和 F2=0x100-(0x02+0x04+0x08)
// ‘00’ Data Rrecord：用来记录数据，HEX文件的大部分记录都是数据记录
//
// ‘01’ End of File Record: 用来标识文件结束，放在文件的最后，标识HEX文件的结尾
//
// ‘02’ Extended Segment Address Record: 用来标识扩展段地址的记录
//
// ‘03’ Start Segment Address Record:开始段地址记录
//
// ‘04’ Extended Linear Address Record: 用来标识扩展线性地址的记录
//
// ‘05’ Start Linear Address Record:开始线性地址记录
//
// 实例：‘02’-->>020000021000EC:
// 数据长度：0x02
// 偏移地址（0ffset_addr）：0x0000
// 数据类型：0x02
// 扩展标识符地址：0x1000
// 校验和：0xEC
// 实际计算地址为：(扩展标识符地址<<4) + offset_addr 即：（0x1000<<4）+0x0000 = 0x10000
//
// 实例：‘04’-->>020000040002F8
//
// 数据长度：0x02
// 偏移地址（0ffset_addr）：0x0000
// 数据类型：0x04
// 扩展线性地址：0x0002
// 校验和：0xF8
// 实际计算地址为：(扩展标识符地址<<16) + offset_addr 即：（0x0002<<16）+0x0000 = 0x20000000
//
// 实例：‘05-->>0400000508000018965
// 数据长度：0x02
// 偏移地址（0ffset_addr）：0x0000
// 数据类型：0x05
// 开始线性地址（即函数入口）：0x080000189
// 校验和：0x65
// 实际计算地址为：开始线性地址+ offset_addr 即：0x080000189+0x0000 = 0x0800001890
// 此类型对烧写程序无用。
//
// 实例： ‘03’ 开始段地址，对于嵌入式HEX无用。

RESULT_STATUS HexFileAddrRange(const char *srcHexFile, uint32_t *startAddr, uint32_t *endAddr);
RESULT_STATUS HexFile2BinFile(const char *srcHexFile, const char *destBinFile,
                              FwFileInfo *fwFileInfo);
RESULT_STATUS MergeHexFile(unsigned int filecount, char srcList[][512], char *destBinFile,
                           char *destHexFile, unsigned char dataLen);
RESULT_STATUS BinFile2HexFile(char *srcBinFile, char *destHexFile, unsigned long startAadr,
                              unsigned char dataLen);

#ifdef __cplusplus
};
#endif

#endif
