#ifndef DEF_H
#define DEF_H

#include <QtGlobal>

/**
 * @brief 上位机向下位机发送的命令
 */
namespace PID_MASTER
{
    enum CMD : quint8
    {
        Target = 0u,    //设置下位机通道的目标值(1个int类型)
        Reset,          //设置下位机复位指令
        Param,          //设置下位机PID值(3个，P、I、D，float类型)
        Start,          //设置下位机启动指令
        Stop,           //设置下位机停止指令
        Cycle,          //设置下位机周期(int类型)
    };
}

/**
 * @brief 下位机向上位机发送的命令
 */
namespace PID_SLAVE
{
    enum CMD : quint8
    {
        Target = 0u,    //设置上位机通道的目标值(1个int类型)
        Actual,         //设置上位机通道实际值
        Param,          //设置上位机PID值(3个，P、I、D，float类型)
        Start,          //设置上位机启动指令(同步上位机的按钮状态)
        Stop,           //设置上位机停止指令(同步上位机的按钮状态)
        Cycle ,         //设置上位机周期(1个int类型)
    };  
}

/**
 * @brief IO设备交互消息
 */
namespace COMMON_MSG
{
    enum MSG : quint8
    {
        OpenFail = 0u,      //打开设备失败
        OpenSuccessful,     //打开设备成功
        ReadyRead,          //可从设备读取数据
        Disconnected,       //设备断开连接
        Connected,          //设备连接成功
        SyncSuccessful,     //同步成功
        TimeOut,            //超时
        ProcessDone,        //处理完成
        ProcessIng,         //处理中
        Log,                //日志打印
    };
}

#endif //DEF_H
