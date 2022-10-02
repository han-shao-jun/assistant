#ifndef UART_H
#define UART_H

#include <QtSerialPort>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QMutex>
#include <QQueue>
#include <QThread>

class Uart : public QThread
{
Q_OBJECT
public:
    explicit Uart(QObject *parent = nullptr);
    ~Uart() override;

    QQueue<QByteArray> recBufferUart, recCopy;   //接收双缓存区
    QQueue<QStringList> recBufferPid;

public slots:
    void recConfig(const QStringList& config);
    void close();
    void write(const QByteArray& sendText);

protected:
    void run() override;

signals:
    void msgSignal(const QString& msg);

private:
    QMutex mutex;
    bool isConnected = false;
    QSerialPort *port;        //将要打开的串口端口
    QString portDes;          //将要打开的串口描述
    QString type;
};

#endif // UART_H
