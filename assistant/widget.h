#ifndef WIDGET_H
#define WIDGET_H

#include "com.h"
#include "convert.h"
#include "def.h"
#include "download.h"
#include "network.h"
#include "osc.h"
#include "pid.h"
#include "serialport_info.h"
#include <QComboBox>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QStringList>
#include <QTextStream>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <QtCore5Compat>

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
    static void HexQString_to_QString(QString &source);
    static void QByteArray_to_HexQByteArray(QByteArray &source);
    void loadStyleSheet(const QString &styleSheetFile);

protected:
    void enterEvent(QEnterEvent *event) override;
    bool eventFilter(QObject *target, QEvent *event) override;

Q_SIGNALS:
    void startComThread(const QStringList &config);
    void closeComThread();
    void sendComDateSignal(const QByteArray &sendText);

    void startPidThread(const QStringList &config);
    void closePidThread();
    void sendPidDateSignal(const QByteArray &sendText);

    void startNetThread(const QStringList &config);
    void closeNetThread();
    void sendNetDateSignal(const QByteArray &sendText);

    void startOscThread(const QStringList &config);
    void closeOscThread();
    void sendOscDateSignal(const QByteArray &sendText);

    void sendDowCmd(const QStringList &config);
    void sendDowPortCmd(const QStringList &config);
    void closeDowThread();

private slots:
    void menuBtnClicked();

    void comMsgHandle(const COMMON_MSG::MSG &msg);
    void comSendData();

    void pidMsgHandle(const COMMON_MSG::MSG &msg);
    void pidSendData(PID_MASTER::CMD cmd);
    void oscMsgHandle(const COMMON_MSG::MSG &msg);
    void oscSendDate();

    void dowMsgHandle(const DOW::TYPE &type, const COMMON_MSG::MSG &msg, const QStringList &arg);

private:
    Ui::Widget *ui;

    void menuInit();
    void comInit();
    void uartInit();
    void pidInit();
    void netInit();
    void oscUiInit();
    void downloadInit();

    void setComUiIsEnabled(bool state);
    void setUartUiIsEnabled(bool state);
    void setPidUiIsEnabled(bool state);

    void oscAddTcpServer(bool state);
    void oscAddUdp(bool state);
    void setOscConfigUiIsEnabled(bool state);
    void oscConnectUiIsEnabled(bool state);
    void dowPackCmd(DOW::TYPE type, DOW::CMD cmd, QStringList &arg);
    void setDowUiIsEnabled(DOW::TYPE type, bool stste);

    int widgetIndex = 0;
    quint64 recBytes = 0;
    quint64 sendBytes = 0;
    SerialPortInfo *serialPortInfo;
    bool comHexFlag = false;
    QTimer *comAutoSendTimer{};
    bool isComConnected = false;

    /******文本通信************/
    Com *comThread{};
    QStringList comConfig;
    QString comSendCycle;

    /*********PID************/
    Pid *pidThread{};
    QStringList pidConfig;
    bool isPidConnected = false;
    bool isPidStart = true;
    typedef struct PidParam {
        QString p, i, d;
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
    } PidParam_t;
    PidParam_t *pidChannel[4]{};
    double pidCycleKey = 0.0;
    double pidCycleKeyStep = 0.020;

    /*****OSC通信*****/

    // TCP控件
    QGridLayout *oscTcpGridLayout{};
    QLabel *oscTcpConClient{};
    QPushButton *oscTcpDisBtn{};
    QComboBox *oscTcpClient{};

    // UDP控件
    QGridLayout *oscUdpGridLayout{};
    QLabel *oscUdpDesLabel{};
    QLineEdit *oscUdpDesIp{};
    QLabel *oscUdpDesPortLabel{};
    QLineEdit *oscUdpDesPort{};
    QHostAddress oscUdpClientIp;

    // 网络通信线程
    OSC *oscThread{};
    bool oscConnectFlag = false;
    QStringList oscConfig; // 通信配置

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
