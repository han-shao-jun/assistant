#include "osc.h"

OSC::OSC(QObject *parent) : QThread(parent)
{
    this->setParent(parent);
    tcpServer = new QTcpServer(this);
    tcpSocketServer = new QTcpSocket(this);
    tcpSocketClient  = new QTcpSocket(this);
    udpSocket = new QUdpSocket(this);

    //建立TCP服务器
    connect(tcpServer, &QTcpServer::newConnection, this, [=]()
    {
        //建立通信套接字并获取客户端信息
        tcpSocketServer = tcpServer->nextPendingConnection();
        tcpClientName = tcpSocketServer->peerName();
        tcpClientIp = tcpSocketServer->peerAddress();
        tcpClientPort = tcpSocketServer->peerPort();

        //接收消息
        connect(tcpSocketServer, &QTcpSocket::readyRead, this, [=]()
        {
            QByteArray temp;
            temp = tcpSocketServer->readAll();
            recCopy.enqueue(temp);    //向尾部插入数据
        });

        //断开连接
        connect(tcpSocketServer, &QTcpSocket::disconnected, this, [=]()
        {
            //关闭套接字
            if (connectFlag)
            {
                connectFlag = false;
                tcpSocketServer->abort();
                tcpSocketServer->close();
                tcpSocketServer->disconnect();
                emit msgSignal(COMMON_MSG::MSG::Disconnected);
            }
        });

        connectFlag = true;
        emit msgSignal(COMMON_MSG::MSG::Connected);
    });

    //连接TCP服务器
    connect(tcpSocketClient, &QTcpSocket::connected, this, [=]()
    {
        connectFlag = true;
        emit msgSignal(COMMON_MSG::MSG::Connected);
    });
    connect(tcpSocketClient, &QTcpSocket::readyRead, this, [=]()
    {
        QByteArray temp;
        temp = tcpSocketClient->readAll();
        recCopy.enqueue(temp);    //向尾部插入数据
    });
    connect(tcpSocketClient, &QTcpSocket::disconnected, this, [=]()
    {
        connectFlag = false;
        emit msgSignal(COMMON_MSG::MSG::Connected);
    });

    //UDP通信
    connect(udpSocket, &QUdpSocket::readyRead, this, [=]()
    {
        QByteArray datagram;
        datagram.resize(udpSocket->pendingDatagramSize());
        udpSocket->readDatagram(datagram.data(), datagram.size());

        //转移数据
        if (!datagram.isEmpty())
        {
            recCopy.enqueue(datagram);
        }
    });
}

OSC::~OSC() = default;


/**
 * @brief 接收UI控件配置信息
 * @param ip IP地址
 * @param port 端口
 * @param Protocol 协议
 */
void OSC::recConfig(const QStringList &config)
{
    qDebug() << config;
    switch (config[0].toInt())
    {
    case 0:
        protocol = "tcpServer";
        tcpServer->listen(QHostAddress(config[1]), config[2].toUInt());
        break;
    case 1:
        protocol = "tcpClient";
        tcpSocketClient->connectToHost(QHostAddress(config[1]), config[2].toUInt());
        break;
    case 2:
        protocol = "udp";
        udpSocket->bind(QHostAddress(config[1]), config[2].toUInt());
        udpClientIp = QHostAddress(config[3]);
        udpClientPort = config[4].toUInt();
        connectFlag = true;
        emit msgSignal(COMMON_MSG::MSG::Connected);
        break;
    default:
        break;
    }
}

/**
 * @brief 线程主体
 */
void OSC::run()
{
    qDebug() << "run"<< QThread::currentThreadId();
    recBuffer.clear();
    recCopy.clear();
    while (connectFlag)
    {
        mutex.lock();
        if (!recCopy.isEmpty())
        {
            recBuffer.enqueue(recCopy.dequeue());
            emit msgSignal(COMMON_MSG::MSG::ReadyRead);
        }
        else
        {
            QThread::usleep(1);     //避免线程占用过多CPU资源
        }
        mutex.unlock();
    }
}

/**
 * @brief 关闭线程
 */
void OSC::close()
{
    connectFlag = false;
    qDebug() << "close"<< QThread::currentThreadId();
    if (protocol == "tcpServer")
    {
        tcpSocketServer->abort();
        tcpSocketServer->close();
        tcpSocketServer->disconnect();
        tcpServer->close();
    }
    else if(protocol == "tcpClient")
    {
        tcpSocketClient->abort();
        tcpSocketClient->close();
    }
    else
    {
        udpSocket->abort();
        udpSocket->close();
    }
}

/**
 * @brief 发送信息
 * @param msg 信息
 */
void OSC::write(const QByteArray& msg)
{
    if (connectFlag)
    {
        if (protocol == "tcpServer")
        {
            tcpSocketServer->write(msg);
        }
        else if (protocol == "tcpClient")
        {
            tcpSocketClient->write(msg);
        }
        else
        {
            udpSocket->writeDatagram(msg, udpClientIp, udpClientPort); //本地虚拟网卡不支持发送
        }
    }
}

/**
 * @brief 获取本地ipAddress字符串列表
 * @note 用于在下拉选项列表中显示本地ipAddress
 * @return ipAddress字符串列表
 */
QStringList OSC:: getAllLocalIp()
{
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
    QStringList ipAddress;

    for(const auto &address: ipAddressesList)
    {
        if (address.protocol() == QAbstractSocket::IPv4Protocol)
        {
            ipAddress.append(address.toString());
        }
    }

    return ipAddress;
}


