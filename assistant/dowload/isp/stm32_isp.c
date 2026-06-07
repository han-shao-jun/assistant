#include "stm32_isp.h"

/**
 * 外部接口，移植时需要实现以下三个接口
 */

/**
 * @brief 串口发送
 * @param buffer 数据缓冲区指针
 * @param len 数据长度(byte)
 * @param timeOut 发送超时时长(ms)
 * @return true:发送成功，flase；发送失败
 */
extern bool SeriPortWrite(uint8_t *buffer, uint32_t len, uint32_t timeOut);

/**
 * @brief 串口接收
 * @param buffer
 * @param len
 * @param timeOut
 * @return
 */
extern uint32_t SeriPortRead(uint8_t *buffer, uint32_t len, uint32_t timeOut);

/**
 *
 */
extern void STM32_GoIntoIsp();


static bool ISP_WaitAck();
static bool ISP_SendCommand(uint8_t cmd);


/**
 * @brief 与stm32 ISP握手同步
 * @note  在使用其他命令之前必须先同步，同步只需要一次即可
 * @return true 同步成功
 *         false 同步失败
 */
bool ISP_Sync()
{
    uint8_t buffer = Sync;

    STM32_GoIntoIsp();

    /*********发送同步数据*************/
    SeriPortWrite(&buffer, 1, 200);
    if (ISP_WaitAck()) {
        return true;
    }
    return false;
}

/**
 * @brief 获取BootLoader版本
 * @param rec 反馈存储区
 * @return true 获取成功
 * @return false 获取失败
 */
bool ISP_GetVersion(uint8_t *rec)
{
    /*********发送命令是否应答？*************/
    if (ISP_SendCommand(Get_Version)) {
        SeriPortRead(rec, 4, 5);
        return true;
    }

    return false;
}

/**
 * @brief 获取PID
 * @param rec 反馈存储区
 * @return true 获取成功
 * @return false 获取失败
 */
bool ISP_GetPID(uint8_t *rec)
{
    /*********发送命令是否应答？*************/
    if (ISP_SendCommand(Get_PID)) {
        SeriPortRead(rec, 4, 5);
        return true;
    }

    return false;
}

/**
 * @brief 擦除FLASH
 * @return true 获取成功
 * @return false 获取失败
 */
bool ISP_Erase()
{
    uint8_t buffer[2] = {0xff, 0x00};
    if (!ISP_SendCommand(Erase)) {
        return false;
    }

    /**********全片擦除************/
    SeriPortWrite(buffer, 2, 100);

    /**********等待擦除************/
    int tryCnt = 100;
    while (tryCnt) {
        if (ISP_WaitAck()) {
            return true;
        }
        tryCnt--;
    }
    return false;
}

/**
 * @brief 执行程序
 * @return true 执行成功
 * @return false 执行失败
 */
bool ISP_Go(uint32_t addr)
{
    uint8_t buffer[5];

    /*********发送命令是否应答？**************/
    if (ISP_SendCommand(Go)) {
        /***********发送地址数据包*************/
        buffer[0] = (addr >> 24) & 0xFF;
        buffer[1] = (addr >> 16) & 0xFF;
        buffer[2] = (addr >> 8) & 0xFF;
        buffer[3] = (addr >> 0) & 0xFF;
        buffer[4] = buffer[0] ^ buffer[1] ^ buffer[2] ^ buffer[3]; //地址校验码
        SeriPortWrite(buffer, 5, 100);
        if (ISP_WaitAck()) {
            return true;
        }
        return false;
    }

    return false;
}

/**
 * @brief 烧录数据
 * @param addr 烧录起始地址
 * @param data 待烧录数据存储区指针
 * @param len 数据长度(byte)
 * @return true 烧录成功
 * @return false 烧录失败
 */
bool ISP_ProgramFlash(uint32_t addr, uint8_t *data, uint32_t len)
{
    uint8_t buffer[5];

    /*********最大长度为256字节**************/
    if (len > 256)
        return false;

    /*********发送命令是否应答？**************/
    if (ISP_SendCommand(Write_Memory)) {
        /***********发送地址数据包*************/
        buffer[0] = (addr >> 24) & 0xFF;
        buffer[1] = (addr >> 16) & 0xFF;
        buffer[2] = (addr >> 8) & 0xFF;
        buffer[3] = (addr >> 0) & 0xFF;
        buffer[4] = buffer[0] ^ buffer[1] ^ buffer[2] ^ buffer[3];//地址校验码
        SeriPortWrite(buffer, 5, 100);

        /*********是否应答？****************/
        if (ISP_WaitAck()) {
            /*********发送数据长度****************/
            buffer[0] = len - 1;
            SeriPortWrite(buffer, 1, 100);

            /*********发送数据****************/
            SeriPortWrite(data, len, 1000);

            /*********发送数据校验和****************/
            uint8_t checksum = buffer[0];
            for (int i = 0; i < len; i++)
                checksum ^= data[i];
            buffer[0] = checksum;
            SeriPortWrite(buffer, 1, 100);

            /*********是否应答？****************/
            if (ISP_WaitAck()) {
                return true;
            }
            return false;
        }
    }

    return true;
}

/**
 * @brief 读取flash数据
 * @return true 读取成功
 * @return false 读取失败
 */
bool ISP_ReadFlash(uint32_t addr, uint8_t *data, uint32_t len)
{
    uint8_t buffer[5];

    /*********发送命令是否应答？**************/
    if (ISP_SendCommand(Read_Memory)) {
        /***********发送地址数据包*************/
        buffer[0] = (addr >> 24) & 0xFF;
        buffer[1] = (addr >> 16) & 0xFF;
        buffer[2] = (addr >> 8) & 0xFF;
        buffer[3] = (addr >> 0) & 0xFF;
        buffer[4] = buffer[0] ^ buffer[1] ^ buffer[2] ^ buffer[3]; //地址校验码
        SeriPortWrite(buffer, 5, 100);

        /*********是否应答？****************/
        if (ISP_WaitAck()) {
            /*********发送读取的长度****************/
            buffer[0] = len - 1;
            buffer[1] = ~(len - 1);
            SeriPortWrite(buffer, 2, 100);

            /*********是否应答？****************/
            if (ISP_WaitAck()) {
                /*********接收数据****************/
                SeriPortRead(data, len, 1000);
                return true;
            }
        }
    }

    return false;
}

/**
 * @brief 等待MCU响应命令
 * @return true 应答
 * @return false 非应答
 */
bool ISP_WaitAck()
{
    uint8_t buffer[2];
    if (SeriPortRead(buffer, 1, 1000)) {
        if (buffer[0] == STM32_ACK) {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

/**
 * @brief 发送命令并等待响应
 * @param cmd 命令
 */
static bool ISP_SendCommand(uint8_t cmd)
{
    uint8_t buffer[2];

    buffer[0] = cmd;
    buffer[1] = ~cmd;
    SeriPortWrite(buffer, 2, 10);

    return ISP_WaitAck();
}
