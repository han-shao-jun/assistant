#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <QtSerialPort>
#include <QThread>
#include <QTimer>
#include <QByteArray>
#include "def.h"
#include "ymodem.h"

#define BEGIN 0x00
#define START 0x01
#define FIRST_SEND  0x02
#define SEND  0x03
#define END_CHECK 0x04
#define END  0x05

/**
 * @brief ISP命令
 */
namespace DOW
{
    enum CMD : quint8
    {
        Open = 0x00, //打开
        Flash = 0x01, //下载
        ReadInfo = 0x02, //读取芯片信息
        Erase = 0x03, //擦除
        Close = 0x04, //关闭
    };
    enum TYPE : quint8
    {
        ISP = 0x00,
        IAP = 0x01,
        OpenOCD = 0x02,
        Jink = 0x03,
        STlink = 0x04,
    };
}

class Download : public QObject
{
Q_OBJECT
public:
    explicit Download(QObject *parent = nullptr);
    ~Download() override;
    QQueue<QByteArray> recBuffer;      //要打印显示的信息
    QQueue<QByteArray> recCopy;        //接收到的原始信息

public slots:
    void doWork(const QStringList& config);


signals:
    void msgSignal(const DOW::TYPE type, const COMMON_MSG::MSG& msg);

protected:

private:
    QMutex mutex;
    bool isConnected = false;
    bool session_done = false;
    QSerialPort *port = nullptr;         //将要打开的串口端口
    QString flowControl;
    quint8 type{}, cmd{};
    QTimer timer;
    QStringList configCopy;
};


#endif //DOWNLOAD_H
