#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include "def.h"
#include "stm32_isp.h"
#include "ymodem.h"
#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtSerialPort/QtSerialPort>

#define BEGIN     0x00
#define START     0x01
#define SEND      0x02
#define END_CHECK 0x03
#define END       0x04

/**
 * @brief ISP命令
 */
namespace DOW
{
enum CMD : quint8
{
    Open = 0x00,     // 打开
    Program = 0x01,  // 编程(烧录)
    ReadInfo = 0x02, // 读取芯片信息
    Erase = 0x03,    // 擦除
    Execute = 0x04,  // 执行程序
    Close = 0x05,    // 关闭
};
enum TYPE : quint8
{
    ISP = 0x00,
    IAP = 0x01,
    OpenOCD = 0x02,
};
} // namespace DOW

class Download : public QObject
{
    Q_OBJECT
public:
    explicit Download(QObject *parent = nullptr);
    ~Download() override;
    QQueue<QByteArray> recBuffer; // 要打印显示的信息

public Q_SLOTS:
    void doWork(const QStringList &config);
    void endSession();

Q_SIGNALS:
    void msgSignal(const DOW::TYPE type, const COMMON_MSG::MSG &msg, const QStringList &arg);

private:
    QMutex mutex;
    bool isConnected = false;
    bool sessionDone = true;
    QSerialPort *port = nullptr; // 将要打开的串口端口
    QString flowControl;
    quint8 type{}, cmd{};
    QStringList arg;

    bool OpenSerialPort(const QString &portName, const int &baudRate);
    void IspOpen(const QString &portName, const int &baudRate);
    void IspReadInfo();
    void IspErase();
    void IspFlash(const QString &filePath);
    void IspExecute(uint32_t addr);
    void IapOpen(const QString &portName, const int &baudRate);
    void IapFlash(const QString &filePath);
};

#endif // DOWNLOAD_H
