#include "convert.h"
#include "Shlwapi.h"
#include <stdio.h>

/**
 * @brief 字符转二进制数
 * @param c 字符
 * @return 转换完的二进制数
 */
static uint8_t HexCharToBinChar(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    else if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return 0xff;
}

/**
 * @brief 两个十六进制字符转换成单个二进制数
 * @param p 两个文本字符
 * @return 转化为1个字节
 */
static uint8_t Hex2Bin(const char *p)
{
    uint8_t tmp = 0;
    tmp = HexCharToBinChar(p[0]);
    tmp <<= 4;
    tmp |= HexCharToBinChar(p[1]);
    return tmp;
}

/**
 * @brief 解析hex文件中的一行字符
 * @param src hex文件单行字符串存储区
 * @param p[out] 二进制格式数据结构指针
 * @return 返回hex格式流分析的结果
 */
static RESULT_STATUS HexFormatDecode(const char *src, BinFarmat *p)
{
    uint8_t check = 0, tmp[4], binLen;
    uint16_t hexLen = strlen(src);
    uint16_t num = 0, offset = 0;
    if (hexLen > HEX_MAX_LENGTH) // 数据内容过长
        return RES_DATA_TOO_LONG;
    if (hexLen < HEX_MIN_LEN)
        return RES_DATA_TOO_SHORT; // 数据内容过短
    if (src[0] != ':')
        return RES_NO_COLON; // 没有冒号
    if ((hexLen - 1) % 2 != 0)
        return RES_LENGTH_ERROR; // hexLen的长度应该为奇数
    binLen = (hexLen - 1) / 2; // bin总数据的长度，包括长度，地址，类型校验等内容
    while (num < 4) {
        offset = (num << 1) + 1;
        tmp[num] = Hex2Bin(src + offset);
        check += tmp[num];
        num++;
    }
    p->len = tmp[0]; // 把解析的这些数据保存到结构体中
    p->addr = tmp[1];
    p->addr <<= 8;
    p->addr += tmp[2];
    p->type = tmp[3];
    while (num < binLen) {
        offset = (num << 1) + 1; // 保存真正的bin格式数据流
        p->data[num - 4] = Hex2Bin(src + offset);
        check += p->data[num - 4];
        num++;
    }
    if (p->len != binLen - 5) // 检查hex格式流的长度和数据的长度是否一致
        return RES_LENGTH_ERROR;
    if (check != 0) // 检查校验是否通过
        return RES_CHECK_ERROR;
    return RES_OK;
}

/**
 * @brief bin文件转换成hex文件
 * @param srcBinFile bin文件名字
 * @param destHexFile hex文件名字
 * @param startAadr 起始地址
 * @param dataLen 文件大小
 * @return 转换结果
 */
RESULT_STATUS BinFile2HexFile(char *srcBinFile, char *destHexFile, unsigned long startAadr,
                              unsigned char dataLen)
{
    FILE *fp_read;             /* 待读取文件句柄 */
    FILE *fp_write;            /* 待写入文件句柄 */
    unsigned short cur_base;   /* 转换成Hex格式的当前地址高16位 */
    unsigned short cur_offset; /* 转换成Hex格式的当前地址低16位 */
    unsigned char read_buf[512];
    char write_buf[1034];

    assert(srcBinFile != NULL && destHexFile != NULL);

    fp_read = fopen(srcBinFile, "rb");
    if (fp_read == NULL) {
        return RES_HEX_FILE_NOT_EXIST;
    }
    fp_write = fopen(destHexFile, "wb");
    if (fp_write == NULL) {
        fclose(fp_read);
        return RES_BIN_FILE_PATH_ERROR;
    }
    cur_base = (unsigned short)(startAadr >> 16);
    cur_offset = (unsigned short)startAadr;

    unsigned char cnt;
    unsigned char read_num;
    unsigned char cksum, highc, lowc;

    /* 设置当前地址高16位 */
    highc = cur_base >> 8;
    lowc = (unsigned char)cur_base;
    cksum = 2 + 4 + highc + lowc;
    cksum = 0xFF - cksum;
    cksum = cksum + 1;
    sprintf(write_buf, ":02000004%04X%02X", cur_base, cksum);
    write_buf[15] = 0x0D;
    write_buf[16] = 0x0A;
    fwrite(write_buf, 1, 17, fp_write);

    read_num = fread(read_buf, 1, dataLen, fp_read);
    while (read_num == dataLen) {
        /* 写入读取的16字节 */
        highc = cur_offset >> 8;
        lowc = (unsigned char)cur_offset;
        cksum = dataLen + highc + lowc;
        for (cnt = 0; cnt < dataLen; cnt++) {
            cksum += read_buf[cnt];
        }
        cksum = 0xFF - cksum;
        cksum = cksum + 1;

        sprintf(write_buf, ":%02X%02X%02X00", dataLen, highc, lowc);
        for (cnt = 0; cnt < read_num; cnt++) {
            sprintf(&write_buf[9 + cnt * 2], "%02X", read_buf[cnt]);
        }
        sprintf(&write_buf[9 + cnt * 2], "%02X", cksum);
        write_buf[11 + dataLen * 2] = 0x0D;
        write_buf[12 + dataLen * 2] = 0x0A;
        fwrite(write_buf, 1, 13 + dataLen * 2, fp_write);

        /* 计算当前地址低16位，当越限时写入当前地址高16位 */
        if (cur_offset == 65536 - dataLen) {
            cur_offset = 0;
            cur_base++;
            highc = cur_base >> 8;
            lowc = (unsigned char)cur_base;
            cksum = 2 + 4 + highc + lowc;
            cksum = 0xFF - cksum;
            cksum = cksum + 1;
            sprintf(write_buf, ":02000004%04X%02X", cur_base, cksum);
            write_buf[15] = 0x0D;
            write_buf[16] = 0x0A;
            fwrite(write_buf, 1, 17, fp_write);
        } else {
            cur_offset += dataLen;
        }

        read_num = fread(read_buf, 1, dataLen, fp_read);
    }

    /* 写入剩余的字节 */
    dataLen = read_num;
    if (dataLen) {
        highc = cur_offset >> 8;
        lowc = (unsigned char)cur_offset;
        cksum = dataLen + highc + lowc;
        for (cnt = 0; cnt < dataLen; cnt++) {
            cksum += read_buf[cnt];
        }
        cksum = 0xFF - cksum;
        cksum = cksum + 1;

        sprintf(write_buf, ":%02X%02X%02X00", dataLen, highc, lowc);
        for (cnt = 0; cnt < read_num; cnt++) {
            sprintf(&write_buf[9 + cnt * 2], "%02X", read_buf[cnt]);
        }
        sprintf(&write_buf[9 + cnt * 2], "%02X", cksum);
        write_buf[11 + dataLen * 2] = 0x0D;
        write_buf[12 + dataLen * 2] = 0x0A;
        fwrite(write_buf, 1, 13 + dataLen * 2, fp_write);
    }

    /* 写入终止序列 */
    sprintf(write_buf, ":00000001FF");
    write_buf[11] = 0x0D;
    write_buf[12] = 0x0A;
    fwrite(write_buf, 1, 13, fp_write);

    fclose(fp_read);
    fclose(fp_write);

    return RES_OK;
}

/**
 * @brief 获取hex文件起始地址结束地址
 * @param srcHexFile hex文件路径
 * @param startAddr[out] 保存起始地址
 * @param endAddr[out] 保存结束地址
 * @return 获取结果
 */
RESULT_STATUS HexFileAddrRange(const char *srcHexFile, uint32_t *startAddr, uint32_t *endAddr)
{
    FILE *src_file;
    uint32_t addr_low = 0;
    uint32_t addr_hign = 0;
    char buffer_hex[600];
    uint8_t buffer_bin[255];
    BinFarmat gBinFor;
    RESULT_STATUS res;
    gBinFor.data = buffer_bin;
    uint32_t TempEndAddr = 0;
    uint32_t TempStartAddr = 0xFFFFFFF;
    int Line = 0;

    assert(srcHexFile != NULL && startAddr != NULL && endAddr != NULL);

    src_file = fopen(srcHexFile, "r"); // 以文本的形式打开一个hex文件
    if (!src_file)
        return RES_HEX_FILE_NOT_EXIST;
    fseek(src_file, 0, SEEK_SET); // 定位到开头，准备开始读取数据
    while (!feof(src_file)) {
        Line++;
        fscanf(src_file, "%s\r\n", buffer_hex);
        res = HexFormatDecode(buffer_hex, &gBinFor);
        if (res != RES_OK) {
            fclose(src_file);
            return res;
        }

        switch (gBinFor.type) {
        case 0: // 数据记录
            addr_low = gBinFor.addr;

            if (TempStartAddr >= addr_hign + addr_low) {
                TempStartAddr = addr_hign + addr_low;
            }

            if (TempEndAddr <= addr_hign + addr_low + gBinFor.len) {
                TempEndAddr = addr_hign + addr_low + gBinFor.len;
            }

            break;
        case 1: // 数据结束
            fclose(src_file);
            *startAddr = TempStartAddr;
            *endAddr = TempEndAddr;
            return RES_OK;
        case 2:
            addr_hign = (((gBinFor.data[0] << 8) + gBinFor.data[1]) << 4);
            break;
        case 4: // 线性段地址
            addr_hign = (((gBinFor.data[0] << 8) + gBinFor.data[1]) << 16);
            break;
        case 5:
        case 3:
            // 未做处理，对烧写无用，不做处理，不加这个分支会报错
            // 未做处理，03’ 开始段地址，对于嵌入式HEX无用。
            break;
        default:
            fclose(src_file);
            return RES_TYPE_ERROR;
        }
    }

    fclose(src_file);
    return RES_TYPE_ERROR;
}

/**
 * @brief hex文件转换成bin文件
 * @param srcHexFile hex文件路径
 * @param destBinFile bin文件路径
 * @return 转换结果
 */
RESULT_STATUS HexFile2BinFile(const char *srcHexFile, const char *destBinFile,
                              FwFileInfo *fwFileInfo)
{
    FILE *src_file, *dest_file;
    uint32_t addr_low = 0;
    uint32_t addr_hign = 0;
    char buffer_hex[600];
    uint8_t buffer_bin[255];
    BinFarmat gBinFor;
    RESULT_STATUS res;
    gBinFor.data = buffer_bin;
    int Line = 0;
    int dataLineCount = 0;
    int addrLineCount = 0;
    uint32_t startAddr = 0;
    uint32_t endAddr = 0;
    uint32_t fileSize;
    int i;
    uint8_t DEFAULT[64];
    memset(DEFAULT, 0x0, 64);
    uint32_t Pages = 0;
    uint32_t Remainder = 0;

    assert(srcHexFile != NULL && destBinFile != NULL && fwFileInfo != NULL);

    res = HexFileAddrRange(srcHexFile, &startAddr, &endAddr);
    fileSize = endAddr - startAddr;
    Pages = fileSize / 64;
    Remainder = fileSize % 64;

    if (res != RES_OK) {
        return RES_TYPE_ERROR;
    } else {
        fwFileInfo->StartAddr = startAddr;
        fwFileInfo->EndAddr = endAddr;
        fwFileInfo->FileSize = fileSize;
        printf("%s %s %s: startAddr=0x%X endAddr=0x%X fileSize=%dbyte\n", __FUNCTION__, srcHexFile,
               destBinFile, startAddr, endAddr, fileSize);
    }

    src_file = fopen(srcHexFile, "r"); // 以文本的形式打开一个hex文件
    if (!src_file)
        return RES_HEX_FILE_NOT_EXIST;
    dest_file = fopen(destBinFile, "wb"); // 以二进制写的方式打开文件，文件不存在也没影响
    if (!dest_file) {
        fclose(src_file);
        return RES_BIN_FILE_PATH_ERROR;
    }

    // 清空文件
    for (i = 0; i < Pages; i++) {
        fwrite((const uint8_t *)DEFAULT, 64, 1, dest_file);
    }
    for (i = 0; i < Remainder; i++) {
        fwrite((const uint8_t *)DEFAULT, 1, 1, dest_file);
    }

    fseek(src_file, 0, SEEK_SET); // 定位到开头，准备开始读取数据
    while (!feof(src_file)) {
        Line++;
        fscanf(src_file, "%s\r\n", buffer_hex); // 读取一行字符
        res = HexFormatDecode(buffer_hex, &gBinFor);
        if (res != RES_OK) {
            fclose(src_file);
            fclose(dest_file);
            return res;
        }
        switch (gBinFor.type) {
        case 0: // 数据记录
            dataLineCount++;
            addr_low = gBinFor.addr;
            // 数据指针偏移
            fseek(dest_file, addr_hign + addr_low - startAddr,
                  SEEK_SET); // addr_hign 这个地址也剔除，数据太大
            if (fwrite((const uint8_t *)gBinFor.data, gBinFor.len, 1, dest_file) != 1) {
                fclose(src_file);
                fclose(dest_file);
                return RES_WRITE_ERROR;
            }
            break;
        case 1: // 数据结束
            fclose(src_file);
            fclose(dest_file);
            return RES_OK;
        case 2: // 这个类型没有调试过，可能有bug
            // 参见网址：http://blog.csdn.net/a1037488611/article/details/43340055
            // 参见网址：http://blog.chinaunix.net/uid-17188120-id-5866369.html
            // 					实例：‘02’-->>020000021000EC:
            // 					数据长度：0x02
            // 					偏移地址（0ffset_addr）：0x0000
            // 					数据类型：0x02
            // 					扩展标识符地址：0x1000
            // 					校验和：0xEC
            // 					实际计算地址为：(扩展标识符地址<<4) + offset_addr
            // 即：（0x1000<<4）+0x0000 = 0x10000
            addr_hign = (((gBinFor.data[0] << 8) + gBinFor.data[1]) << 4);
            addrLineCount++;
            break;

        case 4: // 线性段地址
            // 参见网址：http://blog.csdn.net/a1037488611/article/details/43340055
            // 参见网址：http://blog.chinaunix.net/uid-17188120-id-5866369.html
            // 					实例：‘04’-->>020000040002F8
            //
            // 					数据长度：0x02
            // 					偏移地址（0ffset_addr）：0x0000
            // 					数据类型：0x04
            // 					扩展线性地址：0x0002
            // 					校验和：0xF8
            // 					实际计算地址为：(扩展标识符地址<<16) + offset_addr
            // 即：（0x0002<<16）+0x0000 = 0x20000000
            // 这个才是正确的地址计算方法，但是转BIN文件时需要剔除，不然转化出来的文件太大
            addr_hign = (((gBinFor.data[0] << 8) + gBinFor.data[1]) << 16);
            addrLineCount++;
            break;

        case 5:
        case 3:
            // 未做处理heyonghui，对烧写无用，不做处理，不加这个分支会报错
            // 未做处理heyonghui，03’ 开始段地址，对于嵌入式HEX无用。
            break;
        default:
            fclose(src_file);
            fclose(dest_file);
            return RES_TYPE_ERROR;
        }
    }
    fclose(src_file);
    fclose(dest_file);
    return RES_HEX_FILE_NO_END;
}

/**
 * @brief 合并hex文件
 * @param filecount hex文件数量
 * @param srcList 所有文件路径
 * @param destBinFile 合并之后的Bin文件名字
 * @param destHexFile 合并之后的hex文件名字
 * @param dataLen 文件大小
 * @return 合并结果
 */
RESULT_STATUS MergeHexFile(unsigned int filecount, char srcList[][512], char *destBinFile,
                           char *destHexFile, unsigned char dataLen)
{
    FILE *src_file, *dest_file;
    char *src;
    uint16_t addr_low = 0;
    uint32_t addr_hign = 0;
    char buffer_hex[600];
    uint8_t buffer_bin[255];
    BinFarmat binFarmat;
    RESULT_STATUS res;
    uint32_t maxFileLength = 0;
    uint32_t MinAddr = 0xffffffff;
    uint32_t i, j, k;

    assert(destBinFile != NULL && destHexFile != NULL);

    binFarmat.data = buffer_bin;

    dest_file = fopen(destBinFile, "wb"); // 以二进制写的方式打开文件，文件不存在也没影响
    if (!dest_file)
        return RES_BIN_FILE_PATH_ERROR;

    for (j = 0; j < 2; j++) {
        for (i = 0; i < filecount; i++) {
            src = srcList[i];
            src_file = fopen(src, "r"); // 以文本的形式打开一个hex文件
            if (!src_file) {
                fclose(dest_file);
                return RES_HEX_FILE_NOT_EXIST;
            }
            fseek(src_file, 0, SEEK_SET); // 定位到开头，准备开始读取数据
            int Line = 0;
            int DataLineCount = 0;
            uint16_t First_addr_low = 0;
            int bFileEnd = 0;
            while (!feof(src_file) && bFileEnd != 1) {

                Line++;
                fscanf(src_file, "%s\r\n", buffer_hex);
                res = HexFormatDecode(buffer_hex, &binFarmat);
                if (res != RES_OK) {
                    fclose(src_file);
                    fclose(dest_file);
                    return res;
                }
                switch (binFarmat.type) {
                case 0: // 数据记录
                    DataLineCount++;
                    if (DataLineCount ==
                        1) // 第一行数据获取到起始地址，转bin文件的时候希望从开始就写数据，
                           // 前面一段不希望空着，这里把地址记录下来，后面计算地址用
                    {
                        First_addr_low = binFarmat.addr;
                    }

                    addr_low = binFarmat.addr;
                    // 数据指针偏移
                    if (j == 0) {
                        if (maxFileLength <= (unsigned int)(addr_low + binFarmat.len + addr_hign)) {
                            maxFileLength = addr_low + binFarmat.len + addr_hign;
                        }
                        if (MinAddr >= addr_hign + addr_low) {
                            MinAddr = addr_hign + addr_low;
                        }
                    }
                    if (j == 1) {
                        fseek(dest_file, addr_low + addr_hign - MinAddr,
                              SEEK_SET); // addr_hign 这个地址也剔除，数据太大
                        if (fwrite((const uint8_t *)binFarmat.data, binFarmat.len, 1, dest_file) !=
                            1) {
                            fclose(src_file);
                            fclose(dest_file);
                            // TRACE("fwrite Error\n");
                            return RES_WRITE_ERROR;
                        }
                    }

                    break;
                case 1: // 数据结束
                    fclose(src_file);
                    bFileEnd = 1;
                    break;
                case 2:
                    addr_hign = (((binFarmat.data[0] << 8) + binFarmat.data[1]) << 4);
                    break;
                case 4: // 线性段地址
                    // 这个才是正确的地址计算方法，但是转BIN文件时需要剔除，不然转化出来的文件太大
                    addr_hign = (((binFarmat.data[0] << 8) + binFarmat.data[1]) << 16);
                    break;

                case 5:
                case 3:
                    // 未做处理，对烧写无用，不做处理，不加这个分支会报错
                    // 未做处理，03’ 开始段地址，对于嵌入式HEX无用。
                    break;
                default:
                    // TRACE("RES_TYPE_ERROR\n");
                    fclose(src_file);
                    fclose(dest_file);
                    return RES_TYPE_ERROR;
                }
            }
            fclose(src_file);
        }
        if (j == 0) // 第一个循环全写0xff，Flash擦除后默认值是这个
        {
            maxFileLength = maxFileLength - MinAddr;
            if (maxFileLength % 2 == 1) {
                maxFileLength = maxFileLength + 1;
            }
            uint8_t TempBuf = 0xff;
            fseek(dest_file, SEEK_SET, SEEK_SET);
            for (k = 0; k < maxFileLength; k++) {
                fwrite(&TempBuf, 1, 1, dest_file);
            }
            fseek(dest_file, SEEK_SET, SEEK_SET);
        }
    }
    fclose(dest_file);

    printf("maxFileLength=0x%X   MinAddr=0x%X\n", maxFileLength, MinAddr);

    res = BinFile2HexFile(destBinFile, destHexFile, MinAddr, dataLen);

    return res;
}
