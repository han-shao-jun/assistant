#include "pid.h"

Pid::Pid(QObject *parent) : QThread(parent)
{
    setParent(parent);
    this->setParent(parent);
    port = new QSerialPort(this);
    connect(port, &QSerialPort::readyRead, this, [=]()
    {
        recCopy.enqueue(port->readAll());  //尾部填充数据
    });
}

Pid::~Pid()
= default;

/**
 * @brief 接收串口配置
 * @param config 接收UI控件的配置参数
 */
void Pid::recConfig(const QStringList& config)
{
    qDebug() << config;
    //通信协议
    type = config[0];
    if (type == "uart")
    {
        port->setPortName(config[1].section(':', 0, 0));
        portDes = config[1].section(':', 1, 1);
        port->setBaudRate(config[2].toInt());

        //校验位
        switch (config[3].toInt())
        {
        case 0:
            port->setParity(QSerialPort::NoParity);
            break;
        case 1:
            port->setParity(QSerialPort::OddParity);
            break;
        case 2:
            port->setParity(QSerialPort::EvenParity);
            break;
        default:
            break;
        }

        //数据位
        switch (config[4].toInt())
        {
        case 5:
            port->setDataBits(QSerialPort::Data5);
            break;
        case 6:
            port->setDataBits(QSerialPort::Data6);
            break;
        case 7:
            port->setDataBits(QSerialPort::Data7);
            break;
        case 8:
            port->setDataBits(QSerialPort::Data8);
            break;
        default:
            break;
        }

        //停止位
        switch (config[5].toInt())
        {
        case 0:
            port->setStopBits(QSerialPort::OneStop);
            break;
        case 1:
            port->setStopBits(QSerialPort::OneAndHalfStop);
            break;
        case 2:
            port->setStopBits(QSerialPort::TwoStop);
            break;
        default:
            break;
        }
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


void Pid::run()
{
    qDebug() << "Pid::run";
    recCopy.clear();
    recBuffer.clear();
    while (isConnected)
    {
        mutex.lock();
        if (!recCopy.isEmpty())
        {
            QString recText = recCopy.dequeue();  //获取接收缓冲区的数据
            QStringList strList;
            if (recText.startsWith('(') && recText.endsWith(')'))
            {
                recText.remove(0, 1);
                recText.remove(recText.size() - 1, 1);
                strList = recText.split('|');
                recBuffer.enqueue(strList);
                emit msgSignal("readyRead");       //告诉主线程消息接收完成可以处理
            }
        }
        else
        {
            QThread::usleep(1);     //避免线程占用过多CPU资源
        }
        mutex.unlock();
    }
}


/**
 * @brief 关闭串口设备
 */
void Pid::close()
{
    if (isConnected)
    {
        if (type == "uart")
        {
            port->close();
        }
    }
    isConnected = false;
}

/**
 * @brief 串口发送数据
 * @param sendText 待发送的数据
 */
void Pid::write(const QByteArray& sendText)
{
    if (isConnected)
    {
        if (type == "uart")
        {
            port->write(sendText);
        }
    }
}