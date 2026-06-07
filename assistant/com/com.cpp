#include "com.h"

/**
 * @brief 串口构造函数
 * @param parent 父对象
 */
Com::Com(QObject *parent) : QThread(parent)
{
    this->setParent(parent);
    port = new QSerialPort(this);

    // 串口接收数据
    connect(port, &QSerialPort::readyRead, this, [=]() {
        recCopy.enqueue(port->readAll()); // 尾部填充数据
    });

    tcpServer = new QTcpServer(this);
    tcpSocketClient = new QTcpSocket(this);
    udpSocket = new QUdpSocket(this);

    // 建立TCP服务器
    connect(tcpServer, &QTcpServer::newConnection, this, [=]() {
        // 建立通信套接字并获取客户端信息
        tcpSocketServer = tcpServer->nextPendingConnection();
        tcpClientName = tcpSocketServer->peerName();
        tcpClientIp = tcpSocketServer->peerAddress();
        tcpClientPort = tcpSocketServer->peerPort();

        // 接收消息
        connect(tcpSocketServer, &QTcpSocket::readyRead, this, [=]() {
            recCopy.enqueue(tcpSocketServer->readAll()); // 向尾部插入数据
        });

        // 断开连接
        connect(tcpSocketServer, &QTcpSocket::disconnected, this, [=]() {
            // 关闭套接字
            if (isConnected) {
                isConnected = false;
                tcpSocketServer->abort();
                tcpSocketServer->close();
                tcpSocketServer->disconnect(); // 断开连接的信号
                emit msgSignal(COMMON_MSG::MSG::Disconnected);
            }
        });

        isConnected = true;
        emit msgSignal(COMMON_MSG::MSG::Connected);
    });

    // 连接TCP服务器
    connect(tcpSocketClient, &QTcpSocket::connected, this, [=]() {
        isConnected = true;
        emit msgSignal(COMMON_MSG::MSG::Connected);
    });
    connect(tcpSocketClient, &QTcpSocket::readyRead, this, [=]() {
        recCopy.enqueue(tcpSocketClient->readAll()); // 向尾部插入数据
    });

    // 服务端断开，核主动断开均会触发该信号
    connect(tcpSocketClient, &QTcpSocket::disconnected, this, [=]() {
        if (isConnected) {
            isConnected = false;
            tcpSocketClient->abort();
            tcpSocketClient->close();
            emit msgSignal(COMMON_MSG::MSG::Disconnected);
        }
    });

    // UDP通信
    connect(udpSocket, &QUdpSocket::readyRead, this, [=]() {
        QByteArray datagram;
        datagram.resize(udpSocket->pendingDatagramSize());
        udpSocket->readDatagram(datagram.data(), datagram.size());

        // 转移数据
        if (!datagram.isEmpty()) {
            recCopy.enqueue(datagram);
        }
    });
}

/**
 * @brief 线程析构
 */
Com::~Com() = default;

/**
 * @brief 接收串口配置
 * @param config 接收UI控件的配置参数
 */
void Com::recConfig(const QStringList &config)
{
    // 配置串口
    qDebug() << config;
    int timeOutCnt = 0;

    protocol = config[0].toUInt();
    switch (config[0].toInt()) {
    case COM_TYPE::UART:
        port->setPortName(config[1].section(':', 0, 0));
        portDes = config[0].section(':', 1, 1);
        port->setBaudRate(config[2].toInt());

        // 校验位
        switch (config[3].toInt()) {
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
            port->setParity(QSerialPort::NoParity);
            break;
        }

        // 数据位
        switch (config[4].toInt()) {
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

        // 停止位
        switch (config[5].toInt()) {
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

        if (!port->open(QIODevice::ReadWrite)) {
            isConnected = false;
            emit msgSignal(COMMON_MSG::MSG::OpenFail);
        } else {
            isConnected = true;
            emit msgSignal(COMMON_MSG::MSG::OpenSuccessful);
        }
        break;
    case COM_TYPE::TCP_SERVER:
        tcpServer->listen(QHostAddress(config[1]), config[2].toUInt());
        break;
    case COM_TYPE::TCP_CLIENT:
        tcpSocketClient->connectToHost(QHostAddress(config[1]), config[2].toUInt());
        qDebug() << tcpSocketClient->state();
        break;
    default:
        udpSocket->bind(QHostAddress(config[1]), config[2].toUInt());
        udpClientIp = QHostAddress(config[3]);
        udpClientPort = config[4].toUInt();
        isConnected = true;
        emit msgSignal(COMMON_MSG::MSG::Connected);
        break;
    }
}

/**
 * @brief 线程主体
 */
void Com::run()
{
    qDebug() << "Com::run" << QThread::currentThreadId();
    recCopy.clear();
    recBuffer.clear();
    while (isConnected) {
        if (!recCopy.isEmpty()) {
            recBuffer.enqueue(recCopy.dequeue()); // 取出recCopy头部数据向recBuffer尾部插入
            emit msgSignal(COMMON_MSG::MSG::ReadyRead); // 告诉主线程消息接收完成可以打印显示信息了
        } else {
            QThread::usleep(1); // 避免空操作线程占用过多CPU资源
        }
    }
    qDebug() << "Com::close";
}

/**
 * @brief 关闭通信设备
 */
void Com::close()
{
    switch (protocol) {
    case COM_TYPE::UART:
        if (isConnected) {
            port->close();
        }
        break;
    case COM_TYPE::TCP_SERVER:
        qDebug() << "close TCP_SERVER";
        if (isConnected) {
            tcpSocketServer->abort();
            tcpSocketServer->close();
            tcpSocketServer->disconnect(); // 断开连接的信号
        }
        tcpServer->close();
        break;
    case COM_TYPE::TCP_CLIENT:
        qDebug() << "close TCP_CLIENT";
        if (isConnected) {
            tcpSocketClient->abort();
            tcpSocketClient->close();
        }
        break;
    default:
        udpSocket->abort();
        udpSocket->close();
        break;
    }
    isConnected = false;
}

/**
 * @brief 通信设备发送数据
 * @param sendText 待发送的数据
 */
void Com::write(const QByteArray &sendText)
{
    if (isConnected) {
        switch (protocol) {
        case COM_TYPE::UART:
            port->write(sendText);
            break;
        case COM_TYPE::TCP_SERVER:
            tcpSocketServer->write(sendText);
            break;
        case COM_TYPE::TCP_CLIENT:
            tcpSocketClient->write(sendText);
            break;
        case COM_TYPE::UDP:
            udpSocket->write(sendText);
            break;
        default:
            break;
        }
    }
}
