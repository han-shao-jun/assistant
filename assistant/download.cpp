#include "download.h"

Download::Download(QObject *parent) : QThread(parent)
{
    setParent(parent);
    this->setParent(parent);
    port = new QSerialPort(this);
    connect(port, &QSerialPort::readyRead, this, [=]()
    {
        recCopy.enqueue(port->readAll());  //尾部填充数据
    });
}

Download::~Download() = default;

/**
 * @brief 接收主线程下载通信配置
 * @param config 配置字符串列表
 */
void Download::recConfig(const QStringList& config)
{
    qDebug() << config;
    //通信协议
    type = config[0];
    cmd = config[1];

    if (type == QString("isp")) //ISP串口下载
    {
        flowControl = config[2];
        port->setPortName(config[3].section(':', 0, 0));
        port->setBaudRate(config[4].toInt());        //波特率
        port->setParity(QSerialPort::EvenParity);    //校验位
        port->setDataBits(QSerialPort::Data8);       //数据位
        port->setStopBits(QSerialPort::OneStop);     //停止位
        port->setFlowControl(QSerialPort::NoFlowControl);
        if (!port->open(QIODevice::ReadWrite))
        {
            isConnected = false;
            emit msgSignal("openFail");
        }
        else
        {
            isConnected = true;
            emit msgSignal("openSuccessful");
        }
    }
}

void Download::run()
{
    qDebug() << "Download::run";
    if (isConnected)
    {
        if (type == QString("isp"))
        {
            //流控进入ISP

            //发送同步

            //发送命令

        }
    }
    // port->close();
}


void Download::close()
{

}

