#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include "uart.h"
#include <QThread>
#include <QMessageBox>
#include "network.h"
#include <QTimer>
#include <QMap>
#include <QStringList>
#include <QGridLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QTextCodec>
#include <QFileDialog>
#include "serialportinfo.h"
#include "pid.h"
#include "download.h"


QT_BEGIN_NAMESPACE
namespace Ui
{
    class Widget;
}
QT_END_NAMESPACE

/**
 * @brief 上位机向下位机发送的命令
 */
namespace PID_MASTER
{
    enum CMD : quint8
    {
        Target = 0x01,   //设置下位机通道的目标值(1个int类型)
        Reset  = 0x02,   //设置下位机复位指令
        Param  = 0x03,   //设置下位机PID值(3个，P、I、D，float类型)
        Start  = 0x04,   //设置下位机启动指令
        Stop   = 0x05,   //设置下位机停止指令
        Cycle  = 0x06,   //设置下位机周期(int类型)
    };
}

/**
 * @brief 下位机向上位机发送的命令
 */
namespace PID_SLAVE
{
    enum CMD : quint8
    {
        Target = 0x01,   //设置上位机通道的目标值(1个int类型)
        Actual = 0x02,   //设置上位机通道实际值
        Param  = 0x03,   //设置上位机PID值(3个，P、I、D，float类型)
        Start  = 0x04,   //设置上位机启动指令(同步上位机的按钮状态)
        Stop   = 0x05,   //设置上位机停止指令(同步上位机的按钮状态)
        Cycle  = 0x06,   //设置上位机周期(1个int类型)
    };
}



class Widget : public QWidget
{
Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;
    static void HexQString_to_QString(QString& source);
    static void QByteArray_to_HexQByteArray(QByteArray& source);

signals:
    void startUartThread(const QStringList &config);
    void closeUartThread();
    void sendUartDateSignal(const QByteArray& sendText);

    void startPidThread(const QStringList &config);
    void closePidThread();
    void sendPidDateSignal(const QByteArray& sendText);

    void startNetThread(const QStringList &config);
    void closeNetThread();
    void sendNetDateSignal(const QByteArray& sendText);

    void startDowThread(const QStringList &config);
    void closeDowThread();
    void sendDowDateSignal(const QByteArray& sendText);

private slots:
    void menuBtnClicked();

    void uartMsgHandle(const QString& msg);
    void uartSendData();

    void pidMsgHandle(const QString& msg);
    void pidSendData(PID_MASTER::CMD cmd);

    void netMsgHandle(const QString& msg);
    void netSendDate();

    void dowMsgHandle(const QString& msg);

private:
    Ui::Widget *ui;

    void menuInit();
    void uartInit();
    void pidInit();
    void netInit();
    void downloadInit();

    void setUartUiIsEnabled(bool state);
    void setPidUiIsEnabled(bool state);
    void addTcpServer(bool state);
    void addUdp(bool state);
    void setNetConfigUiIsEnabled(bool state);
    void netConnectUiIsEnabled(bool state);
    void ispSendCmd(ISP::CMD cmd);

    int widgetIndex = 0;
    int widgetIndexLast = 0;
    quint64 recBytes = 0;
    quint64 sendBytes = 0;

    SerialPortInfo *serialPortInfo;

    /******串口************/
    Uart *uartThread{};
    bool uartHexFlag = false;
    bool isUartConnected = false;
    QStringList uartConfig;
    QTimer *uartAutoSendTimer{};
    QString uartSendCycle;
    
    /*********PID************/
    Pid *pidThread{};
    bool isPidConnected = false;
    bool isPidStart = true;
    typedef struct PidParam
    {
        QString p,i,d;
        QString target, cycle;
        bool isDisplayActual;
        PidParam()
        {
            p = "0.56";
            i = "0.56";
            d = "0.56";
            target = "100";
            cycle = "20";
            isDisplayActual = false;
        }
    }PidParam_t;
    PidParam_t *pidChannel[4]{};
    double pidCycleKey = 0.0;
    double pidCycleKeyStep = 0.020;

    /*****网络通信*****/
    //网络通信UI管理
    QString netSendCycle;

    //TCP控件
    QGridLayout *netTcpGridLayout{};
    QLabel *netTcpConClient{};
    QPushButton *netTcpDisBtn{};
    QComboBox *netTcpClient{};

    //UDP控件
    QGridLayout *netUdpGridLayout{};
    QLabel *netUdpDesLabel{};
    QLineEdit *netUdpDesIp{};
    QLabel *netUdpDesPortLabel{};
    QLineEdit *netUdpDesPort{};
    QHostAddress udpClientIp;

    //网络通信线程
    Network *netThread{};
    bool netHexFlag = false;
    bool netConnectFlag = false;
    QStringList netConfig;
    QTimer *netAutoSendTimer{};

    /*********下载**************/
    Download *dowThread;

};

#endif // WIDGET_H
