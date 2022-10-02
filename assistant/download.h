#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <QtSerialPort>
#include <QThread>
#include <QTimer>
#include <QByteArray>

/**
 * @brief ISP命令
 */
namespace ISP
{
    enum CMD : quint8
    {
        Flash    = 0x01, //下载
        ReadInfo = 0x02, //读取芯片信息
        Erase    = 0x03, //擦除
    };
}

class Download : public QThread
{
Q_OBJECT
public:
    explicit Download(QObject *parent = nullptr);
    ~Download() override;

    QQueue<QByteArray> recCopy;   //接收双缓存区

public slots:

    void recConfig(const QStringList& config);
    void close();

signals:
    void msgSignal(const QString& msg);

protected:
    void run() override;

private:
    QMutex mutex;
    QSerialPort *port;        //将要打开的串口端口
    QString portDes;          //将要打开的串口描述
    bool isConnected = false;
    QString type, cmd, flowControl;
    QTimer timer;
};


#endif //DOWNLOAD_H
