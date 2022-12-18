#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <QtSerialPort>
#include <QStringList>
#include <QWidget>

#ifdef Q_OS_WINDOWS

#include <windows.h>
#include <dbt.h>
#include <devguid.h>
#include <setupapi.h>
#include <initguid.h>

#endif

class SerialPortInfo : public QWidget
{
Q_OBJECT
public:
    explicit SerialPortInfo(QWidget *parent = nullptr);
    ~SerialPortInfo() override;
    void registerUsingSerialPort(const QString& comport);
    bool unregisterUsingSerialPort(const QString& comport);
    static QStringList availablePorts();
Q_SIGNALS:
    void update(const QStringList& ports);
    void disconnected(const QString& port);

protected:
    bool nativeEvent(const QByteArray& eventType, void *message, long *result) override;
    void registerEvent();

private:
    QStringList portsAvailable; //当前可用串口列表
    QStringList portsUsing;     //正在被使用的串口列表
};


#endif //SERIALPORT_H
