#ifndef _NETWORK_H
#define _NETWORK_H

#include <QNetworkInterface>
#include <QTcpSocket>
#include <QTcpServer>
#include <QUdpSocket>
#include <QQueue>
#include <QThread>
#include <QUdpSocket>
#include <QMutex>
#include <QVector>

class Network: public QThread
{
    Q_OBJECT


public:
    explicit Network(QObject *parent  = nullptr);
    ~Network() override;
    static QStringList getAllLocalIp();
    QString protocol;
    QTcpServer *tcpServer;
    QTcpSocket *tcpSocketServer;
//    QVector<QTcpSocket*> tcpSocketServer;
    quint8 client_num = 0;
    QTcpSocket *tcpSocketClient;
    QUdpSocket *udpSocket;
    bool connectFlag  = false;
    QQueue<QByteArray> recBuffer;      //要打印显示的信息
    QQueue<QByteArray> recCopy;        //接收到的原始数据

    QString tcpClientName;
    QHostAddress tcpClientIp;
    quint16  tcpClientPort;

    QHostAddress udpClientIp;
    quint16  udpClientPort;

signals:
    void msgSignal(const QString& signal);

public slots:
    void recConfig(const QStringList& config);
    void close();
    void write(const QByteArray& msg);

protected:
    void run() override;

private:
    QMutex mutex;
};


#endif //_NETWORK_H
