#ifndef PID_H
#define PID_H

#include "def.h"
#include <QtCore/QMutex>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QThread>
#include <QtSerialPort/QtSerialPort>

class Pid : public QThread
{
    Q_OBJECT
public:
    explicit Pid(QObject *parent = nullptr);
    QQueue<QByteArray> recCopy; // 接收双缓存区
    QQueue<QStringList> recBuffer;
    ~Pid() override;

public slots:
    void recConfig(const QStringList &config);
    void close();
    void write(const QByteArray &sendText);

signals:
    void msgSignal(const COMMON_MSG::MSG &msg);

protected:
    void run() override;

private:
    QMutex mutex;
    QSerialPort *port; // 将要打开的串口端口
    QString portDes;   // 将要打开的串口描述
    bool isConnected = false;
    QString type;
};

#endif // PID_H
