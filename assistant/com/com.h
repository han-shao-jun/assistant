#ifndef COM_H
#define COM_H

#include "def.h"
#include <QtCore/QDebug>
#include <QtCore/QMutex>
#include <QtCore/QQueue>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QUdpSocket>
#include <QtSerialPort/QtSerialPort>

class Com : public QThread
{
    Q_OBJECT
public:
    explicit Com(QObject *parent = nullptr);
    ~Com() override;

    QTcpServer *tcpServer;
    QTcpSocket *tcpSocketServer = nullptr;
    quint8 client_num = 0;
    QTcpSocket *tcpSocketClient;
    QUdpSocket *udpSocket;

    QString tcpClientName;
    QHostAddress tcpClientIp;
    quint16 tcpClientPort;

    QQueue<QByteArray> recBuffer, recCopy; // 接收双缓存区
    unsigned int protocol = 0;

public slots:
    void recConfig(const QStringList &config);
    void close();
    void write(const QByteArray &sendText);

protected:
    void run() override;

signals:
    void msgSignal(const COMMON_MSG::MSG &msg);

private:
    QMutex mutex;
    bool isConnected = false;
    QSerialPort *port; // 将要打开的串口端口
    QString portDes;   // 将要打开的串口描述
    QHostAddress udpClientIp;
    quint16 udpClientPort{};
};

#endif // COM_H
