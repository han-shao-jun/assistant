#include "ymodem.h"

/*
*********************************************************************************************************
*	                                   Ymodem文件传输协议介绍
*********************************************************************************************************
*/
/*
第1阶段： 同步
    从机给数据发送同步字符 C

第2阶段：发送第1帧数据，包含文件名和文件大小
    主机发送：
    ---------------------------------------------------------------------------------------
    | SOH  |  序号 - 0x00 |  序号取反 - 0xff | 128字节数据，含文件名和文件大小字符串|CRC0 CRC1|
    |-------------------------------------------------------------------------------------|
    从机接收：
    接收成功回复ACK和CRC16，接收失败（校验错误，序号有误）继续回复字符C，超过一定错误次数，回复两个CA，终止传输。

第3阶段：数据传输
    主机发送：
    ---------------------------------------------------------------------------------------
    | SOH/STX  |  从0x01开始序号  |  序号取反 | 128字节或者1024字节                |CRC0 CRC1|
    |-------------------------------------------------------------------------------------|
    从机接收：
    接收成功回复ACK，接收失败（校验错误，序号有误）或者用户处理失败继续回复字符C，超过一定错误次数，回复两个CA，终止传输。

第4阶段：结束帧
    主机发送：发送EOT结束传输。
    从机接收：回复ACK。

第5阶段：空帧，结束通话
    主机发送：一帧空数据。
    从机接收：回复ACK。
*/

/**
 * @brief 将整数转换成字符字面量
 * @param intNum 待转换的数字
 * @param str 字符保存区
 */
void Ymodem_Int2Str(uint32_t intNum, uint8_t *str)
{
    uint32_t i, div = 1000000000, j = 0, status = 0;

    for (i = 0; i < 10; i++) {
        str[j++] = (intNum / div) + 48;

        intNum = intNum % div;
        div /= 10;
        if ((str[j - 1] == '0') & (status == 0)) {
            j = 0;
        } else {
            status++;
        }
    }
}

/**
 * @brief  将一个字符串转换为一个整数
 * @param  inputStr: 待转换字符串保存区
 * @param  intNum: 转换 后的结果
 * @retval true: 转换成功
 *         false: 转换失败
 */
bool Ymodem_Str2Int(const uint8_t *inputStr, uint32_t *intNum)
{
    uint32_t i = 0;
    bool res = false;
    uint32_t val = 0;

    if ((inputStr[0] == '0') && ((inputStr[1] == 'x') || (inputStr[1] == 'X'))) {
        i = 2;
        while ((i < 11) && (inputStr[i] != '\0')) {
            if (ISVALIDHEX(inputStr[i])) {
                val = (val << 4) + CONVERTHEX(inputStr[i]);
            } else {
                /* Return 0, Invalid input */
                res = false;
                break;
            }
            i++;
        }

        /* valid result */
        if (inputStr[i] == '\0') {
            *intNum = val;
            res = true;
        }
    } else /* max 10-digit decimal input */
    {
        while ((i < 11) && !res) {
            if (inputStr[i] == '\0') {
                *intNum = val;
                /* return 1 */
                res = true;
            } else if (((inputStr[i] == 'k') || (inputStr[i] == 'K')) && (i > 0)) {
                val = val << 10;
                *intNum = val;
                res = true;
            } else if (((inputStr[i] == 'm') || (inputStr[i] == 'M')) && (i > 0)) {
                val = val << 20;
                *intNum = val;
                res = true;
            } else if (ISVALIDDEC(inputStr[i])) {
                val = val * 10 + CONVERTDEC(inputStr[i]);
            } else {
                /* return 0, Invalid input */
                res = false;
                break;
            }
            i++;
        }
    }

    return res;
}

/**
 * @brief 计算一串数据的CRC
 * @param data 数据保存区
 * @param size  以字节为单位数据长度
 * @return CRC计算结果
 */
uint16_t Ymodem_Cal_CRC16(const uint8_t *data, uint32_t size)
{
    uint16_t crcInit = 0x0000;
    uint16_t crcPoly = 0x1021;
    uint8_t i;

    while (size--) {
        crcInit = (*data << 8) ^ crcInit;
        for (i = 0; i < 8; ++i) {
            if (crcInit & 0x8000)
                crcInit = (crcInit << 1) ^ crcPoly;
            else
                crcInit = crcInit << 1;
        }
        data++;
    }
    return crcInit;
}

/**
 * @brief 计算一串数据总和
 * @param data 数据保存区
 * @param size 数据长度
 * @return 计算结果的后8位
 */
uint8_t Ymodem_CalChecksum(const uint8_t *data, uint32_t size)
{
    uint32_t sum = 0;
    const uint8_t *dataEnd = data + size;

    while (data < dataEnd)
        sum += *data++;

    return (sum & 0xffu);
}

/**
 * @brief 准备第一包要发送的数据
 * @param data 数据保存区
 * @param fileName 固件文件名
 * @param length 以字节为单位文件大小
 */
void Ymodem_PrepareIntialPacket(uint8_t *data, const char *fileName, const uint32_t length)
{
    uint16_t i, j;
    uint8_t fileSize[FILE_SIZE_LENGTH] = {0};
    uint16_t crc;

    /* 第一包数据的前三个字符  */
    data[0] = SOH; /*soh表示数据包是128字节 */
    data[1] = 0;
    data[2] = 0xff;

    /* 文件名 */
    for (i = 0; (fileName[i] != '\0') && (i < FILE_NAME_LENGTH); i++) {
        data[PACKET_HEADER + i] = fileName[i];
    }

    // 文件名结束位
    data[PACKET_HEADER + i] = 0x00;

    /* 文件大小转换成字符 */
    Ymodem_Int2Str(length, fileSize);
    for (j = 0, i = PACKET_HEADER + i + 1; fileSize[j] != '\0'; j++, i++) {
        data[i] = fileSize[j];
    }

    /* 其余补0 */
    for (; i < PACKET_SIZE + PACKET_HEADER; i++) {
        data[i] = 0;
    }

    crc = Ymodem_Cal_CRC16((data + PACKET_HEADER), PACKET_SIZE);
    data[i] = (crc >> 8);         // 高位
    data[i + 1] = (crc & 0x00ff); // 低位
}

/**
 * @brief 准备发送数据包
 * @param sourceBuf 要发送的原数据保存区
 * @param data 最终要发送的数据包保存区，已经包含的头文件和原数据
 * @param pktNo 数据包序号
 * @param sizeBlk 以字节为单位要发送的数据数量
 */
void Ymodem_PrepareDataPacket(const uint8_t *sourceBuf, uint8_t *data, uint8_t pktNo,
                              uint32_t sizeBlk)
{
    uint32_t i, packetSize;
    uint16_t crc;

    /* 设置好要发送数据包的前三个字符data[0]，data[1]，data[2] */
    /* 根据sizeBlk的大小设置数据区数据个数是取1024字节还是取128字节*/
    packetSize = sizeBlk > PACKET_SIZE ? PACKET_1K_SIZE : PACKET_SIZE;

    /* 首字节：确定是1024字节还是用128字节 */
    if (packetSize == PACKET_1K_SIZE) {
        data[0] = STX;
    } else {
        data[0] = SOH;
    }

    /* 第2个字节：数据序号 */
    data[1] = (pktNo);
    /* 第3个字节：数据序号取反 */
    data[2] = (~pktNo);

    /* 填充要发送的原始数据 */
    for (i = 0; i < sizeBlk; ++i) {
        data[PACKET_HEADER + i] = sourceBuf[i];
    }

    /* 不足的补 EOF (0x1A) 或 0x00 */
    for (i = sizeBlk + PACKET_HEADER; i < packetSize + PACKET_HEADER; i++) {
        data[i] = 0x1A; /* EOF (0x1A) or 0x00 */
    }

    crc = Ymodem_Cal_CRC16((data + PACKET_HEADER), packetSize);
    data[i] = (crc >> 8);         // 高位
    data[i + 1] = (crc & 0x00ff); // 低位
}

/**
 * @brief 准备结束数据包
 * @param data  最终要发送的数据包保存区，已经包含的头文件和原数据
 */
void Ymodem_PrepareEndPacket(uint8_t *data)
{
    uint16_t i;
    uint16_t crc;

    /* 数据的前三个字符  */
    data[0] = SOH; /*soh表示数据包是128字节 */
    data[1] = 0;
    data[2] = 0xff;

    /* 其余补0 */
    for (i = PACKET_HEADER; i < PACKET_SIZE + PACKET_HEADER; i++) {
        data[i] = 0;
    }

    crc = Ymodem_Cal_CRC16(data + PACKET_HEADER, PACKET_SIZE);
    data[i + PACKET_HEADER] = (crc >> 8);         // 高位
    data[i + PACKET_HEADER + 1] = (crc & 0x00ff); // 低位
}
