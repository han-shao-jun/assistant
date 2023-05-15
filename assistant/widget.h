#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>

#include <QThread>
#include <QMessageBox>

#include <QTimer>
#include <QMap>
#include <QStringList>
#include <QGridLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QFileDialog>
#include <QVector>
#include <QThread>
#include <QStandardItemModel>
#include "uart.h"
#include "pid.h"
#include "network.h"
#include "serialportinfo.h"
#include "download.h"
#include "def.h"
#include "osc.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class Widget;
}
QT_END_NAMESPACE


class Widget : public QWidget
{
Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;
    static void HexQString_to_QString(QString& source);
    static void QByteArray_to_HexQByteArray(QByteArray& source);
    void loadStyleSheet(const QString& styleSheetFile);

protected:
    void enterEvent(QEvent *event);

Q_SIGNALS:
    void startUartThread(const QStringList &config);
    void closeUartThread();
    void sendUartDateSignal(const QByteArray& sendText);

    void startPidThread(const QStringList &config);
    void closePidThread();
    void sendPidDateSignal(const QByteArray& sendText);

    void startNetThread(const QStringList &config);
    void closeNetThread();
    void sendNetDateSignal(const QByteArray& sendText);

    void startOscThread(const QStringList &config);
    void closeOscThread();
    void sendOscDateSignal(const QByteArray& sendText);

    void sendDowCmd(const QStringList &config);
    void sendDowPortCmd(const QStringList &config);
    void closeDowThread();


private slots:
    void menuBtnClicked();

    void uartMsgHandle(const COMMON_MSG::MSG& msg);
    void uartSendData();

    void pidMsgHandle(const COMMON_MSG::MSG& msg);
    void pidSendData(PID_MASTER::CMD cmd);

    void netMsgHandle(const COMMON_MSG::MSG& msg);
    void netSendDate();

    void oscMsgHandle(const COMMON_MSG::MSG& msg);
    void oscSendDate();

    void dowMsgHandle(const DOW::TYPE& type, const COMMON_MSG::MSG& msg, const QStringList& arg);

private:
    Ui::Widget *ui;

    void menuInit();
    void uartInit();
    void pidInit();
    void netInit();
    void oscUiInit();
    void downloadInit();

    void setUartUiIsEnabled(bool state);
    void setPidUiIsEnabled(bool state);
    void addTcpServer(bool state);
    void addUdp(bool state);
    void setNetConfigUiIsEnabled(bool state);
    void netConnectUiIsEnabled(bool state);
    void oscAddTcpServer(bool state);
    void oscAddUdp(bool state);
    void setOscConfigUiIsEnabled(bool state);
    void oscConnectUiIsEnabled(bool state);
    void dowPackCmd(DOW::TYPE type, DOW::CMD cmd, QStringList& arg);
    void setDowUiIsEnabled(DOW::TYPE type, bool stste);

    int widgetIndex = 0;
    bool isMax = false;
    QSize winSize;
    QRect location;
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
    QStringList pidConfig;
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

    /*****OSC通信*****/

    //TCP控件
    QGridLayout *oscTcpGridLayout{};
    QLabel *oscTcpConClient{};
    QPushButton *oscTcpDisBtn{};
    QComboBox *oscTcpClient{};

    //UDP控件
    QGridLayout *oscUdpGridLayout{};
    QLabel *oscUdpDesLabel{};
    QLineEdit *oscUdpDesIp{};
    QLabel *oscUdpDesPortLabel{};
    QLineEdit *oscUdpDesPort{};
    QHostAddress oscUdpClientIp;

    //网络通信线程

    OSC *oscThread{};
    bool oscConnectFlag = false;
    QStringList oscConfig;  //通信配置

    /*********下载**************/
    QThread *dowThread;
    Download *dowThreadWork;
    bool ispConnected = false;
    bool ispProcessIng = false;
    bool iapConnected = false;
    bool iapProcessIng = false;
    QStringList dowCmdArg;

};

#endif // WIDGET_H
