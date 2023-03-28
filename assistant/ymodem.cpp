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


Ymodem::Ymodem()
= default;

Ymodem::~Ymodem()
= default;

/*
*********************************************************************************************************
*	函 数 名: Int2Str
*	功能说明: 将整数转换成字符
*	形    参: str 字符  intnum 整数
*	返 回 值: 无
*********************************************************************************************************
*/
void Ymodem::Int2Str(uint8_t *str, uint32_t intnum)
{
    uint32_t i, Div = 1000000000, j = 0, Status = 0;

    for (i = 0; i < 10; i++)
    {
        str[j++] = (intnum / Div) + 48;

        intnum = intnum % Div;
        Div /= 10;
        if ((str[j - 1] == '0') & (Status == 0))
        {
            j = 0;
        }
        else
        {
            Status++;
        }
    }
}

/*
*********************************************************************************************************
*	函 数 名: PrepareIntialPacket
*	功能说明: 准备第一包要发送的数据
*	形    参: data 数据
*             fileName 文件名
*             length   文件大小
*	返 回 值: 0
*********************************************************************************************************
*/
void Ymodem::PrepareIntialPacket(QByteArray& data, const char *fileName, const uint32_t length)
{
    uint16_t i, j;
    uint8_t file_ptr[10];
    QString len_str;
    uint8_t payload[128];
    uint16_t crc = 0;

    /* 第一包数据的前三个字符  */
    data[0] = SOH;   /*soh表示数据包是128字节 */
    data[1] = 0;
    data[2] = static_cast<char>(0xff);

    /* 文件名 */
    for (i = 0; (fileName[i] != '\0') && (i < FILE_NAME_LENGTH); i++)
    {
        data[i + PACKET_HEADER] = fileName[i];
    }

    //结束位
    data[i + PACKET_HEADER] = 0x00;
    i = i + PACKET_HEADER + 1;

    /* 文件大小转换成字符 */
    len_str = QString::number(length, 16);
    data.append(len_str);
    i += len_str.size();

    /* 其余补0 */
    for (j = i; j < PACKET_SIZE + PACKET_HEADER; j++)
    {
        data[j] = 0;
    }

    for (i = 0; i < PACKET_SIZE; ++i)
    {
        payload[i] = static_cast<uint8_t>(data[i + PACKET_HEADER]);
    }

    crc = Ymodem::Cal_CRC16(payload, PACKET_SIZE);
    data[i + PACKET_HEADER] = static_cast<char>(crc >> 8);        //高位
    data[i + PACKET_HEADER + 1] = static_cast<char>(crc & 0x00ff);   //低位
}

/*
*********************************************************************************************************
*	函 数 名: PrepareDataPacket
*	功能说明: 准备发送数据包
*	形    参: SourceBuf 要发送的原数据
*             data      最终要发送的数据包，已经包含的头文件和原数据
*             pktNo     数据包序号
*             sizeBlk   要发送数据数
*	返 回 值: 无
*********************************************************************************************************
*/
void Ymodem::PrepareDataPacket(QByteArray& SourceBuf, QByteArray& data, uint8_t pktNo, uint32_t sizeBlk)
{
    uint16_t i, size, packetSize;
    uint8_t payload[PACKET_1K_SIZE];
    uint16_t crc = 0;

    /* 设置好要发送数据包的前三个字符data[0]，data[1]，data[2] */
    /* 根据sizeBlk的大小设置数据区数据个数是取1024字节还是取128字节*/
    packetSize = sizeBlk > PACKET_SIZE ? PACKET_1K_SIZE : PACKET_SIZE;

    /* 数据大小进一步确定 */
    size = sizeBlk < packetSize ? sizeBlk : packetSize;

    /* 首字节：确定是1024字节还是用128字节 */
    if (packetSize == PACKET_1K_SIZE)
    {
        data[0] = STX;
    }
    else
    {
        data[0] = SOH;
    }

    /* 第2个字节：数据序号 */
    data[1] = static_cast<char>(pktNo);
    /* 第3个字节：数据序号取反 */
    data[2] = static_cast<char>(~pktNo);

    /* 填充要发送的原始数据 */
    data.append(SourceBuf);

    /* 不足的补 EOF (0x1A) 或 0x00 */
    if (size <= packetSize)
    {
        for (i = size + PACKET_HEADER; i < packetSize + PACKET_HEADER; i++)
        {
            data[i] = 0x1A; /* EOF (0x1A) or 0x00 */
        }
    }

    for (i = 0; i < packetSize; ++i)
    {
        payload[i] = static_cast<uint8_t>(data[i + PACKET_HEADER]);
    }

    crc = Ymodem::Cal_CRC16(payload, packetSize);
    data[i + PACKET_HEADER] = static_cast<char>(crc >> 8);           //高位
    data[i + PACKET_HEADER + 1] = static_cast<char>(crc & 0x00ff);   //低位
}

void Ymodem::PrepareEndPacket(QByteArray& data)
{
    uint16_t i;
    uint8_t payload[PACKET_SIZE];
    uint16_t crc = 0;

    /* 数据的前三个字符  */
    data[0] = SOH;   /*soh表示数据包是128字节 */
    data[1] = 0;
    data[2] = static_cast<char>(0xff);

    /* 其余补0 */
    for (i = PACKET_HEADER; i < PACKET_SIZE + PACKET_HEADER; i++)
    {
        data[i] = 0;
    }

    for (i = 0; i < PACKET_SIZE; ++i)
    {
        payload[i] = static_cast<uint8_t>(data[i + PACKET_HEADER]);
    }

    crc = Ymodem::Cal_CRC16(payload, PACKET_SIZE);
    data[i + PACKET_HEADER] = static_cast<char>(crc >> 8);    //高位
    data[i + PACKET_HEADER + 1] = static_cast<char>(crc & 0x00ff);   //低位
}


/*
*********************************************************************************************************
*	函 数 名: UpdateCRC16
*	功能说明: 上次计算的CRC结果 crcIn 再加上一个字节数据计算CRC
*	形    参: crcIn 上一次CRC计算结果
*             byte  新添加字节
*	返 回 值: 无
*********************************************************************************************************
*/
uint16_t Ymodem::UpdateCRC16(uint16_t crcIn, uint8_t byte)
{
    uint32_t crc = crcIn;
    uint32_t in = byte | 0x100;

    do
    {
        crc <<= 1;
        in <<= 1;
        if (in & 0x100)
            ++crc;
        if (crc & 0x10000)
            crc ^= 0x1021;
    } while (!(in & 0x10000));

    return crc & 0xffffu;
}

/*
*********************************************************************************************************
*	函 数 名: Cal_CRC16
*	功能说明: 计算一串数据的CRC
*	形    参: data  数据
*             size  数据长度
*	返 回 值: CRC计算结果
*********************************************************************************************************
*/
uint16_t Ymodem::Cal_CRC16(const uint8_t *data, uint32_t size)
{
//    uint32_t crc = 0;
//    const uint8_t *dataEnd = data + size;
//
//    while (data < dataEnd)
//        crc = UpdateCRC16(crc, *data++);
//
//    crc = UpdateCRC16(crc, 0);
//    crc = UpdateCRC16(crc, 0);
//
//    return crc & 0xffffu;
    uint16_t crc_init = 0x0000;
    uint16_t crc_poly = 0x1021;
    uint8_t i = 0;

    while (size--)
    {
        crc_init = (*data << 8) ^ crc_init;
        for (i = 0; i < 8; ++i)
        {
            if (crc_init & 0x8000)
                crc_init = (crc_init << 1) ^ crc_poly;
            else
                crc_init = crc_init << 1;
        }
        data++;
    }
    return crc_init;
}

/*
*********************************************************************************************************
*	函 数 名: CalChecksum
*	功能说明: 计算一串数据总和
*	形    参: data  数据
*             size  数据长度
*	返 回 值: 计算结果的后8位
*********************************************************************************************************
*/
uint8_t Ymodem::CalChecksum(const uint8_t *data, uint32_t size)
{
    uint32_t sum = 0;
    const uint8_t *dataEnd = data + size;

    while (data < dataEnd)
        sum += *data++;

    return (sum & 0xffu);
}


