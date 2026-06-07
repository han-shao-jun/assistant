#ifndef _OSC_H
#define _OSC_H

#include "def.h"
#include <QtCore/QMutex>
#include <QtCore/QQueue>
#include <QtCore/QThread>
#include <QtCore/QVector>
#include <QtNetwork/QNetworkInterface>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QUdpSocket>

class OSC : public QThread
{
    Q_OBJECT

public:
    explicit OSC(QObject *parent = nullptr);
    ~OSC() override;
    static QStringList getAllLocalIp();
    QString protocol;
    QTcpServer *tcpServer;
    QTcpSocket *tcpSocketServer;
    quint8 client_num = 0;
    QTcpSocket *tcpSocketClient;
    QUdpSocket *udpSocket;
    bool connectFlag = false;
    QQueue<QByteArray> recBuffer; // 要打印显示的信息
    QQueue<QByteArray> recCopy;   // 从网卡接收到的原始数据

    QString tcpClientName;
    QHostAddress tcpClientIp;
    quint16 tcpClientPort;

    QHostAddress udpClientIp;
    quint16 udpClientPort{};

signals:
    void msgSignal(const COMMON_MSG::MSG &signal);

public slots:
    void recConfig(const QStringList &config);
    void close();
    void write(const QByteArray &msg);

protected:
    void run() override;

private:
    QMutex mutex;
};

#endif //_OSC_H
