#include "widget.h"
#include "ui_widget.h"
#include <cstdlib>

Widget::Widget(QWidget *parent)
        : QWidget(parent), ui(new Ui::Widget)
{
    ui->setupUi(this);
    qDebug() << "main id: " << QThread::currentThreadId();
    setWindowTitle(tr("多功能调试助手"));

    qRegisterMetaType<COMMON_MSG::MSG>("COMMON_MSG::MSG"); //注册元类型，用于信号与槽传递数据
    qRegisterMetaType<DOW::CMD>("DOW::CMD");               //注册元类型，用于信号与槽传递数据
    qRegisterMetaType<DOW::TYPE>("DOW::TYPE");             //注册元类型，用于信号与槽传递数据

    serialPortInfo = new SerialPortInfo(this);

    /**************新建串口线程*********************************/
    uartThread = new Uart();
    connect(this, &Widget::startUartThread, uartThread, &Uart::recConfig);  //主线程启动串口线程
    connect(this, &Widget::closeUartThread, uartThread, &Uart::close);      //主线程结束串口线程
    connect(this, &Widget::sendUartDateSignal, uartThread, &Uart::write);   //主线程向串口线程发送数据    
    connect(uartThread, &Uart::msgSignal, this, &Widget::uartMsgHandle);    //主线程接收串口线程数据


    /**********PID绘图4个通道********************************/
    pidChannel[0] = new PidParam_t;
    pidChannel[1] = new PidParam_t;
    pidChannel[2] = new PidParam_t;
    pidChannel[3] = new PidParam_t;
    pidChannel[0]->isDisplayActual = true;

    /**************新建PID页面线程*********************************/
    pidThread = new Pid();
    connect(this, &Widget::startPidThread, pidThread, &Pid::recConfig);  //主线程启动串口线程
    connect(this, &Widget::closePidThread, pidThread, &Pid::close);      //主线程结束串口线程
    connect(this, &Widget::sendPidDateSignal, pidThread, &Pid::write);   //主线程向串口线程发送数据
    connect(pidThread, &Pid::msgSignal, this, &Widget::pidMsgHandle);    //主线程接收串口线程数据


    /***********新建网络通信页面线程**********************************/
    netThread = new Network();
    connect(this, &Widget::startNetThread, netThread, &Network::recConfig); //主线程启动网络通信线程
    connect(this, &Widget::closeNetThread, netThread, &Network::close);     //主线程结束网络通信线程
    connect(this, &Widget::sendNetDateSignal, netThread, &Network::write);  //主线程想网络通信线程发送数据
    connect(netThread, &Network::msgSignal, this, &Widget::netMsgHandle);   //主线程接收网络通信线程数据

    /***********新建OSC页面线程**********************************/
    oscThread = new OSC();
    connect(this, &Widget::startOscThread, oscThread, &OSC::recConfig); // 主线程启动网络通信线程
    connect(this, &Widget::closeOscThread, oscThread, &OSC::close);     // 主线程结束网络通信线程
    connect(this, &Widget::sendOscDateSignal, oscThread, &OSC::write);  // 主线程向网络通信线程发送数据
    connect(oscThread, &OSC::msgSignal, this, &Widget::oscMsgHandle);   // 主线程接收网络通信线程数据

    /***********新建下载页面线程**********************************/
    dowThread = new QThread();      //下载子线程
    dowThreadWork = new Download(); //工作类
    dowThreadWork->moveToThread(dowThread);

    connect(this, &Widget::sendDowCmd, dowThreadWork, &Download::doWork); //主线程触发线程
    connect(dowThreadWork, &Download::msgSignal, this, &Widget::dowMsgHandle);   //主线程接收下载线程消息
    dowThread->start();

    /***********记录当前选择页面与上一次选择页面*******************/
    widgetIndex = ui->mainWidget->currentIndex();
    widgetIndexLast = widgetIndex;
    connect(ui->mainWidget, &QStackedWidget::currentChanged, this, [=]()
    {
        widgetIndexLast = widgetIndex;
        widgetIndex = ui->mainWidget->currentIndex();
        if (isUartConnected)
        {
            ui->uartOperateBtn->click();
        }
        if (isPidConnected)
        {
            ui->pidOperateBtn->click();
        }
    });

    this->menuInit();
    this->uartInit();
    this->pidInit();
    this->netInit();
    this->oscUiInit();
    this->downloadInit();

    connect(ui->clearCounterBtn, &QPushButton::clicked, this, [=]()
    {
        recBytes = 0;
        sendBytes = 0;
        ui->recLabel->setText(tr("接收字节数:0"));
        ui->sendLabel->setText(tr("发送字节数:0"));
    });
}

Widget::~Widget()
{
    addTcpServer(false);
    delete netTcpConClient;
    delete netTcpDisBtn;
    delete netTcpClient;
    delete netTcpGridLayout;

    addUdp(false);
    delete netUdpDesLabel;
    delete netUdpDesIp;
    delete netUdpDesPortLabel;
    delete netUdpDesPort;
    delete netUdpGridLayout;

    if (isUartConnected)
    {
        emit closeUartThread();
        uartThread->quit();
        uartThread->wait();
    }
    delete uartThread;

    if (isPidConnected)
    {
        emit closePidThread();
        pidThread->quit();
        pidThread->wait();
    }
    delete pidThread;
    delete pidChannel[0];
    delete pidChannel[1];
    delete pidChannel[2];
    delete pidChannel[3];

    delete serialPortInfo;

    if (netConnectFlag)
    {
        emit closeNetThread();
        netThread->quit();
        netThread->wait();
    }
    delete netThread;

    oscAddTcpServer(false);
    delete oscTcpConClient;
    delete oscTcpDisBtn;
    delete oscTcpClient;
    delete oscTcpGridLayout;

    oscAddUdp(false);
    delete oscUdpDesLabel;
    delete oscUdpDesIp;
    delete oscUdpDesPortLabel;
    delete oscUdpDesPort;
    delete oscUdpGridLayout;

    if (oscConnectFlag)
    {
        emit closeOscThread();
        oscThread->quit();
        oscThread->wait();
    }
    delete oscThread;

    dowThread->quit();
    dowThread->wait();
    delete dowThread;
    delete dowThreadWork;

    delete ui;
    qDebug() << "delete ui";
}

/**
 * @brief 菜单栏初始化
 */
void Widget::menuInit()
{
    connect(ui->menuUartBtn, &QPushButton::clicked, this, &Widget::menuBtnClicked);
    connect(ui->menuPidBtn, &QPushButton::clicked, this, &Widget::menuBtnClicked);
    connect(ui->menuNetBtn, &QPushButton::clicked, this, &Widget::menuBtnClicked);
    connect(ui->menuCameraBtn, &QPushButton::clicked, this, &Widget::menuBtnClicked);
    connect(ui->menuBluetoothBtn, &QPushButton::clicked, this, &Widget::menuBtnClicked);
    connect(ui->menuIspBtn, &QPushButton::clicked, this, &Widget::menuBtnClicked);
    connect(ui->menuTopBtn, &QPushButton::clicked, this, [=]()
    {
        static bool isTop = false;
        if (!isTop)
        {
#ifdef Q_OS_WINDOWS
            //调用windows api 置顶层
            ::SetWindowPos((HWND)this->winId(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            isTop = true;
#endif
        }
        else
        {
#ifdef Q_OS_WINDOWS
            //调用windows api 取消置顶层
            ::SetWindowPos((HWND)this->winId(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            isTop = false;
#endif
        }
    });
}

/**
 * @brief 菜单栏按钮槽函数
 */
void Widget::menuBtnClicked()
{
    QString objName = sender()->objectName();
    int index = 0;
    if (objName == "menuUartBtn")
    {
        index = 0;
    }
    else if (objName == "menuPidBtn")
    {
        index = 1;
    }
    else if (objName == "menuNetBtn")
    {
        index = 2;
    }
    else if (objName == "menuCameraBtn")
    {
        index = 3;
    }
    else if (objName == "menuBluetoothBtn")
    {
        index = 4;
    }
    else if (objName == "menuIspBtn")
    {
        index = 5;
    }
    else
    {
        return;
    }
    ui->mainWidget->setCurrentIndex(index);
}


/**
 * @brief 初始化串口助手界面
 */
void Widget::uartInit()
{
    /*************调整串口界面*******************************/
    ui->uartBaudRate->setCurrentIndex(6);
    ui->uartDataBit->setCurrentIndex(3);
    ui->uartSendBtn->setDisabled(true);

    /**************填充可用串口*****************************/
    auto ports = SerialPortInfo::availablePorts();
    ui->uartPortName->clear();
    ui->uartPortName->addItems(ports);
    connect(serialPortInfo, &SerialPortInfo::update, this, [=](const QStringList& ports) //刷新可用串口
    {
        ui->uartPortName->clear();
        ui->uartPortName->addItems(ports);
    });
    connect(serialPortInfo, &SerialPortInfo::disconnected, this, [=](const QString& port)
    {
        if (isUartConnected && port == ui->uartPortName->currentText()) //串口断开
        {
            uartAutoSendTimer->stop();
            isUartConnected = false;
            emit closeUartThread();
            uartThread->quit();
            uartThread->wait();
            QMessageBox::warning(this, tr("警告"), tr("串口断开连接"));
            setUartUiIsEnabled(true);  //改变控件的状态
            ui->uartOperateBtn->setText(tr("打开串口")); //改变按钮文字
        }
    });

    /*************限制输入框内容**************************/
    QRegExp regExp(R"(^([1-9][0-9]{1,5})?$)");  //正则表达式(注意原始字面量)
    ui->uartSendCycleEdit->setValidator(new QRegExpValidator(regExp, this));

    /************串口界面控按钮信号与槽连接*****************************/
    connect(ui->uartOperateBtn, &QPushButton::clicked, this, [=]()          //打开串口按钮
    {
        if (!isUartConnected)   //打开串口
        {
            if (ui->uartPortName->count() != 0)
            {
                uartConfig.clear();
                uartConfig.append(ui->uartPortName->currentText());
                uartConfig.append(ui->uartBaudRate->currentText());
                uartConfig.append(QString::number(ui->uartCheckBit->currentIndex()));
                uartConfig.append(ui->uartDataBit->currentText());
                uartConfig.append(QString::number(ui->uartStopBit->currentIndex()));
                serialPortInfo->registerUsingSerialPort(ui->uartPortName->currentText());
                emit startUartThread(uartConfig);
            }
            else
            {
                QMessageBox::warning(this, tr("错误"), tr("当前无串口设备"));
            }
        }
        else  //关闭串口
        {
            isUartConnected = false;
            emit closeUartThread();
            uartThread->quit();
            uartThread->wait();
            setUartUiIsEnabled(true);
            serialPortInfo->unregisterUsingSerialPort(ui->uartPortName->currentText());
            ui->uartOperateBtn->setText(tr("打开串口"));
        }
    });
    connect(ui->uartRecHexBtn, &QRadioButton::clicked, this, [=]()          //串口接收区16进制显示
    {
        if (!uartHexFlag)
        {
            uartHexFlag = true;
            QByteArray text = ui->uartRecText->toPlainText().toUtf8();      //获取已经显示的文本
            QByteArray_to_HexQByteArray(text);
            ui->uartRecText->setText(text);
        }
    });
    connect(ui->uartRecAsciiBtn, &QPushButton::clicked, this, [=]()         //串口接收区字符显示
    {
        if (uartHexFlag)
        {
            uartHexFlag = false;
            QString text = ui->uartRecText->toPlainText(); //获取已经显示的文本
            HexQString_to_QString(text);
            ui->uartRecText->setText(text);
        }
    });
    connect(ui->uartRecSaveBtn, &QPushButton::clicked, this, [=]()  //接收区保存到文件
    {
        QString date = ui->uartRecText->toPlainText();
        if (date.isNull())
        {
            QMessageBox::warning(this, tr("错误"), tr("接收区没有数据"));
            return;
        }
        QString path = QFileDialog::getSaveFileName(this, tr("保存文件"), QDir::home().path(), "Text files (*.txt)");
        if (path.isNull())
        {
            return;
        }
        QFile file(path);               //参数就是文件的路径
        file.open(QIODevice::WriteOnly | QIODevice::Text); //设置打开方式
        QTextStream text(&file);
        text << date;
        file.close();                   //关闭文件对象
    });
    connect(ui->uartRecClearBtn, &QPushButton::clicked, this, [=]()         //清除串口接收区
    {
        ui->uartRecText->clear();
    });
    connect(ui->uartSendFileBtn, &QPushButton::clicked, this, [=]()         //发送区选择文件
    {
        QString path = QFileDialog::getOpenFileName(this, "选择文件", QDir::home().path(), "Text files (*.txt)");
        if (path.size() == 0)
        {
            return;
        }
        QFile file(path);                   //参数就是文件的路径
        file.open(QIODevice::ReadOnly);     //设置打开方式
        QByteArray array = file.readAll();  //用QByteArray类去接收读取的信息
        ui->uartSendText->setText(array);   //将读取到的数据放入textEdit中
        file.close();                       //关闭文件对象
    });
    connect(ui->uartSendClearBtn, &QPushButton::clicked, this, [=]()        //发送区清除
    {
        ui->uartSendText->clear();
    });
    connect(ui->uartSendBtn, &QPushButton::clicked, this, &Widget::uartSendData);   //串口发送区发送数据并处理


    /**************串口定时自动发送***********************/
    uartAutoSendTimer = new QTimer(this);
    connect(ui->uartSendAutoBtn, &QCheckBox::stateChanged, this, [=]()          //串口定时发送按钮
    {
        if (ui->uartSendAutoBtn->isChecked())
        {
            qint32 time = ui->uartSendCycleEdit->text().toInt();
            if (time != 0 && isUartConnected)
            {
                uartAutoSendTimer->start(time);
            }
        }
        else
        {
            uartAutoSendTimer->stop();
        }
    });
    uartSendCycle = ui->uartSendCycleEdit->text();
    connect(ui->uartSendCycleEdit, &QLineEdit::editingFinished, this, [=]()     //串口定时自动发送周期编辑栏
    {
        if (ui->uartSendCycleEdit->text().isNull())
        {
            ui->uartSendCycleEdit->setText(uartSendCycle);
        }
        uartSendCycle = ui->uartSendCycleEdit->text();
        if (ui->uartSendAutoBtn->isChecked() && isUartConnected)
        {
            qint32 time = ui->uartSendCycleEdit->text().toInt();
            if (time != 0)
            {
                uartAutoSendTimer->start(time);
            }
        }
    });
    connect(uartAutoSendTimer, &QTimer::timeout, this, [=]()                    //串口定时自动发送
    {
        if (isUartConnected)
        {
            uartSendData();
        }
    });
}

/**
 * @brief 串口UI控件是否可用
 * @param state 状态
 */
void Widget::setUartUiIsEnabled(bool state)
{
    ui->uartPortName->setEnabled(state);
    ui->uartBaudRate->setEnabled(state);
    ui->uartCheckBit->setEnabled(state);
    ui->uartDataBit->setEnabled(state);
    ui->uartStopBit->setEnabled(state);

    ui->uartStateBtn->setChecked(!state);
    ui->uartSendBtn->setDisabled(state);
}

/**
 * @brief 处理串口线程的信号
 * @param msg 收到的信号
 */
void Widget::uartMsgHandle(const COMMON_MSG::MSG& msg)
{
    if (msg == COMMON_MSG::MSG::ReadyRead) //接收到数据
    {
        if (ui->uartRecStopBtn->checkState() != Qt::Checked) //暂停接收按钮没有被按下
        {
            QByteArray recText = uartThread->recBufferUart.dequeue();   //获取接收缓冲区的数据
            recBytes += recText.length();                           //追加接收到的字符字节数量
            ui->recLabel->setText(QString("接收字节数:%1").arg(recBytes));
            if (ui->uartRecHexBtn->isChecked())                     //判断是否显示16进制
            {
                QByteArray_to_HexQByteArray(recText);
            }
            ui->uartRecText->moveCursor(QTextCursor::End);  //移动光标到末尾
            ui->uartRecText->insertPlainText(recText);  //文末追加文本
        }
        else
        {
            uartThread->recBufferUart.clear();
        }
    }
    else if (msg == COMMON_MSG::MSG::OpenFail) //打开失败
    {
        QMessageBox::warning(this, tr("警告"), tr("无法打开串口"));
        isUartConnected = false;
    }
    else if (msg == COMMON_MSG::MSG::OpenSuccessful) //打开成功
    {
        setUartUiIsEnabled(false);                  //改变控件的状态
        ui->uartOperateBtn->setText(tr("关闭串口"));  //改变按钮文字
        if (ui->uartSendAutoBtn->isChecked())       //定时发送按钮被选中
        {
            uartAutoSendTimer->start(ui->uartSendCycleEdit->text().toInt());
        }
        uartThread->start();
        isUartConnected = true;
    }
}

/**
 * @brief 串口发送数据
 */
void Widget::uartSendData()
{
    QByteArray sendText;
    sendText = ui->uartSendText->toPlainText().toUtf8();

    if (ui->uartSendNewlineBtn->checkState() == Qt::Checked)     //发送新行被选中
        sendText += '\n';

    if (ui->uartSendHexBtn->isChecked())                         //16进制发送
        sendText = sendText.toHex();

    emit sendUartDateSignal(sendText);
    sendBytes += sendText.length();                             //累加发送字符字节数
    ui->sendLabel->setText(QString("发送字节数:%1").arg(sendBytes));
}


/**
 * @brief 初始化PID界面
 */
void Widget::pidInit()
{
    /**********设置默认的串口配置**************************************************/
    ui->pidBaudRate->setCurrentIndex(6);
    ui->pidDataBit->setCurrentIndex(3);

    /**************填充可用串口*****************************/
    auto ports = SerialPortInfo::availablePorts();
    ui->pidPortName->clear();
    ui->pidPortName->addItems(ports);
    connect(serialPortInfo, &SerialPortInfo::update, this, [=](const QStringList& ports) //刷新可用串口
    {
        ui->pidPortName->clear();
        ui->pidPortName->addItems(ports);
    });
    connect(serialPortInfo, &SerialPortInfo::disconnected, this, [=](const QString& port)
    {
        if (isPidConnected && port == ui->pidPortName->currentText()) //串口断开
        {
            uartAutoSendTimer->stop();
            isPidConnected = false;
            emit closePidThread();
            uartThread->quit();
            uartThread->wait();
            QMessageBox::warning(this, tr("警告"), tr("串口断开连接"));
            setPidUiIsEnabled(true);                    //改变控件的状态
            ui->pidOperateBtn->setText(tr("打开串口"));  //改变按钮文字
        }
    });

    /*************用正则表达式限制输入框内容***********************************************/
    QRegExp regExp(R"(^((\-?)(\d{1,6})(\.?)(\d{1,6}))?$)"); //正则表达式(注意使用原始字面量)
    ui->pidPEdit->setValidator(new QRegExpValidator(regExp, this));
    ui->pidIEdit->setValidator(new QRegExpValidator(regExp, this));
    ui->pidDEdit->setValidator(new QRegExpValidator(regExp, this));
    regExp.setPattern(R"(^([1-9][0-9]{1,5})?$)");   //正则表达式(注意使用原始字面量)
    ui->pidTargetEdit->setValidator(new QRegExpValidator(regExp, this));
    ui->pidCycleEdit->setValidator(new QRegExpValidator(regExp, this));

    this->setPidUiIsEnabled(true);

    /******************保证输入数据非空**************************************/
    connect(ui->pidPEdit, &QLineEdit::editingFinished, this, [=]()
    {
        if (ui->pidPEdit->text().isNull())
        {
            ui->pidPEdit->setText(pidChannel[ui->pidChannelBtn->currentIndex()]->p);
        }
        pidChannel[ui->pidChannelBtn->currentIndex()]->p = ui->pidPEdit->text();
    });
    connect(ui->pidIEdit, &QLineEdit::editingFinished, this, [=]()
    {
        if (ui->pidIEdit->text().isNull())
        {
            ui->pidIEdit->setText(pidChannel[ui->pidChannelBtn->currentIndex()]->i);
        }
        pidChannel[ui->pidChannelBtn->currentIndex()]->i = ui->pidIEdit->text();
    });
    connect(ui->pidDEdit, &QLineEdit::editingFinished, this, [=]()
    {
        if (ui->pidDEdit->text().isNull())
        {
            ui->pidDEdit->setText(pidChannel[ui->pidChannelBtn->currentIndex()]->d);
        }
        pidChannel[ui->pidChannelBtn->currentIndex()]->d = ui->pidDEdit->text();
    });
    connect(ui->pidTargetEdit, &QLineEdit::editingFinished, this, [=]()
    {
        if (ui->pidTargetEdit->text().isNull())
        {
            ui->pidTargetEdit->setText(pidChannel[ui->pidChannelBtn->currentIndex()]->target);
        }
        pidChannel[ui->pidChannelBtn->currentIndex()]->target = ui->pidTargetEdit->text();
    });
    connect(ui->pidCycleEdit, &QLineEdit::editingFinished, this, [=]()
    {
        if (ui->pidCycleEdit->text().isNull())
        {
            ui->pidCycleEdit->setText(pidChannel[ui->pidChannelBtn->currentIndex()]->cycle);
        }
        pidChannel[ui->pidChannelBtn->currentIndex()]->cycle = ui->pidCycleEdit->text();
        pidCycleKeyStep = pidChannel[ui->pidChannelBtn->currentIndex()]->cycle.toDouble() / 1000.0;
    });


    /****************按钮槽连接**************************************************/
    connect(ui->pidOperateBtn, &QPushButton::clicked, this, [=]()          //打开串口按钮
    {
        if (!isPidConnected)   //打开串口
        {
            if (ui->pidPortName->count() != 0)
            {
                QStringList config;
                config.append(QString("uart"));
                config.append(ui->pidPortName->currentText());
                config.append(ui->pidBaudRate->currentText());
                config.append(QString::number(ui->pidCheckBit->currentIndex()));
                config.append(ui->uartDataBit->currentText());
                config.append(QString::number(ui->pidStopBit->currentIndex()));
                serialPortInfo->registerUsingSerialPort(ui->pidPortName->currentText());
                emit startPidThread(config);
            }
            else
            {
                QMessageBox::warning(this, tr("错误"), tr("当前无串口设备"));
            }
        }
        else  //关闭串口
        {
            isPidConnected = false;
            emit closePidThread();
            uartThread->quit();
            uartThread->wait();
            ui->pidStartBtn->click();
            setPidUiIsEnabled(true);
            serialPortInfo->unregisterUsingSerialPort(ui->pidPortName->currentText());
            ui->pidOperateBtn->setText(tr("打开串口"));
        }
    });
    connect(ui->pidSendTargetBtn, &QPushButton::clicked, this, [=]()            //发送目标按钮
    {
        this->pidSendData(PID_MASTER::Target);
    });
    connect(ui->pidResetBtn, &QPushButton::clicked, this, [=]()                 //复位按钮
    {
        this->pidSendData(PID_MASTER::Reset);
    });
    connect(ui->pidSendParamBtn, &QPushButton::clicked, this, [=]()             //发送PID按钮
    {
        this->pidSendData(PID_MASTER::Param);
    });
    connect(ui->pidStartBtn, &QPushButton::clicked, this, [=]()                 //启动与停止按钮
    {
        qDebug() << isPidStart;
        if (isPidConnected)
        {
            if (isPidStart)
            {
                this->pidSendData(PID_MASTER::Start);
                ui->pidStartBtn->setText(tr("停止"));
            }
            else
            {
                this->pidSendData(PID_MASTER::Stop);
                ui->pidStartBtn->setText(tr("启动"));
            }
            isPidStart = !isPidStart;
        }
        else if (!isPidStart)
        {
            ui->pidStartBtn->setText(tr("启动"));
            isPidStart = !isPidStart;
        }
    });
    connect(ui->pidSendCycleBtn, &QPushButton::clicked, this, [=]()     //发送周期按钮
    {
        this->pidSendData(PID_MASTER::Cycle);
    });


    /****************曲线处理********************************/
    ui->customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iMultiSelect);   //可拖拽+可滚轮缩放+可以被选中

    /**************横坐标以时间为刻度**************************/
    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%h:%m:%s");  //显示格式 时：分：秒
    ui->customPlot->xAxis->setTicker(timeTicker);
    ui->customPlot->xAxis->setRange(0.0, 5.0);  //最小单位是秒
    ui->customPlot->yAxis->setRange(0.0, 100);

    /****************添加四条曲线*****************************/
    ui->customPlot->addGraph();                         //添加一条曲线
    ui->customPlot->graph(0)->setPen(QPen(Qt::green));  //曲线蓝色
    ui->customPlot->graph(0)->setName("CH1");           //图例名称
    ui->customPlot->addGraph();
    ui->customPlot->graph(1)->setPen(QPen(Qt::blue));
    ui->customPlot->graph(1)->setName("CH2");
    ui->customPlot->addGraph();
    ui->customPlot->graph(2)->setPen(QPen(Qt::yellow));
    ui->customPlot->graph(2)->setName("CH3");
    ui->customPlot->addGraph();
    ui->customPlot->graph(3)->setPen(QPen(Qt::magenta));
    ui->customPlot->graph(3)->setName("CH4");

    /*************曲线图例设置****************************/
    ui->customPlot->legend->setFillOrder(QCPLayoutGrid::foColumnsFirst);//设置图例优先按排放置
    ui->customPlot->legend->setWrap(4);         //优先规则中最大数量(一排最多数量或则一列最多数量，取决于优先放置规则)
    ui->customPlot->plotLayout()->addElement(1, 0, ui->customPlot->legend); //图例和坐标图按栅格布局，前两个参数控制元素在栅格的位置
    ui->customPlot->plotLayout()->setRowStretchFactor(1, 0.01); //设置显示比例
    ui->customPlot->legend->setBorderPen(Qt::NoPen);    //设置边框隐藏
    ui->customPlot->legend->setVisible(true);           //图例可见

    /*************面板控件设置曲线*****************************/
    connect(ui->pidClearDisplayBtn, &QPushButton::clicked, this, [=]()  //清除所有曲线数据点并还原坐标
    {
        ui->customPlot->graph(0)->data().data()->clear();   //获取数据容器并清除数据
        ui->customPlot->graph(1)->data().data()->clear();
        ui->customPlot->graph(2)->data().data()->clear();
        ui->customPlot->graph(3)->data().data()->clear();
        ui->customPlot->xAxis->setRange(0, 5);   //还原横坐标
        ui->customPlot->replot();
        pidCycleKey = 0.0;
    });
    connect(ui->pidActualCH1, &QCheckBox::clicked, this, [=]()      //通道1实际值显示
    {
        if (ui->pidActualCH1->checkState() == Qt::Checked)
        {
            ui->customPlot->graph(0)->setVisible(true);
        }
        else
        {
            ui->customPlot->graph(0)->setVisible(false);
        }
        ui->customPlot->replot();
    });
    connect(ui->pidActualCH2, &QCheckBox::clicked, this, [=]()      //通道2实际值显示
    {
        if (ui->pidActualCH2->checkState() == Qt::Checked)
        {
            ui->customPlot->graph(1)->setVisible(true);
        }
        else
        {
            ui->customPlot->graph(1)->setVisible(false);
        }
        ui->customPlot->replot();
    });
    connect(ui->pidActualCH3, &QCheckBox::clicked, this, [=]()      //通道3实际值显示
    {
        if (ui->pidActualCH3->checkState() == Qt::Checked)
        {
            ui->customPlot->graph(2)->setVisible(true);
        }
        else
        {
            ui->customPlot->graph(2)->setVisible(false);
        }
        ui->customPlot->replot();
    });
    connect(ui->pidActualCH4, &QCheckBox::clicked, this, [=]()      //通道4实际值显示
    {
        if (ui->pidActualCH4->checkState() == Qt::Checked)
        {
            ui->customPlot->graph(3)->setVisible(true);
        }
        else
        {
            ui->customPlot->graph(3)->setVisible(false);
        }
        ui->customPlot->replot();
    });

    /******************目标值标尺线**********************************/
    auto *targetLineCH1 = new QCPItemLine(ui->customPlot);
    targetLineCH1->setPen(QPen(Qt::red));
    targetLineCH1->start->setType(QCPItemPosition::ptPlotCoords);
    targetLineCH1->start->setCoords(0, pidChannel[0]->target.toDouble());
    targetLineCH1->end->setType(QCPItemPosition::ptPlotCoords);
    targetLineCH1->end->setCoords(10000, pidChannel[0]->target.toDouble());
    targetLineCH1->setVisible(true);
    connect(ui->pidTargetCH1, &QCheckBox::clicked, this, [=]()
    {
        if (ui->pidTargetCH1->checkState() == Qt::Checked)
        {
            targetLineCH1->start->setCoords(0, pidChannel[0]->target.toDouble());
            targetLineCH1->end->setCoords(10000, pidChannel[0]->target.toDouble());
            targetLineCH1->setVisible(true);
        }
        else
        {
            targetLineCH1->setVisible(false);
        }
        ui->customPlot->replot();
    });

    auto *targetLineCH2 = new QCPItemLine(ui->customPlot);
    targetLineCH2->setPen(QPen(Qt::lightGray));
    targetLineCH2->start->setType(QCPItemPosition::PositionType::ptPlotCoords);
    targetLineCH2->start->setCoords(0, pidChannel[1]->target.toDouble());
    targetLineCH2->end->setType(QCPItemPosition::PositionType::ptPlotCoords);
    targetLineCH2->end->setCoords(10000, pidChannel[1]->target.toDouble());
    targetLineCH2->setVisible(false);
    connect(ui->pidTargetCH2, &QCheckBox::clicked, this, [=]()
    {
        if (ui->pidTargetCH2->checkState() == Qt::Checked)
        {
            targetLineCH2->start->setCoords(0, pidChannel[1]->target.toDouble());
            targetLineCH2->end->setCoords(10000, pidChannel[1]->target.toDouble());
            targetLineCH2->setVisible(true);
        }
        else
        {
            targetLineCH2->setVisible(false);
        }
        ui->customPlot->replot();
    });

    auto *targetLineCH3 = new QCPItemLine(ui->customPlot);
    targetLineCH3->setPen(QPen(Qt::darkBlue));
    targetLineCH3->start->setType(QCPItemPosition::PositionType::ptPlotCoords);
    targetLineCH3->start->setCoords(0, pidChannel[2]->target.toDouble());
    targetLineCH3->end->setType(QCPItemPosition::PositionType::ptPlotCoords);
    targetLineCH3->end->setCoords(10000, pidChannel[2]->target.toDouble());
    targetLineCH3->setVisible(false);
    connect(ui->pidTargetCH3, &QCheckBox::clicked, this, [=]()
    {
        if (ui->pidTargetCH3->checkState() == Qt::Checked)
        {
            targetLineCH3->start->setCoords(0, pidChannel[2]->target.toDouble());
            targetLineCH3->end->setCoords(10000, pidChannel[2]->target.toDouble());
            targetLineCH3->setVisible(true);
        }
        else
        {
            targetLineCH3->setVisible(false);
        }
        ui->customPlot->replot();
    });

    auto *targetLineCH4 = new QCPItemLine(ui->customPlot);
    targetLineCH4->setPen(QPen(Qt::darkCyan));
    targetLineCH4->start->setType(QCPItemPosition::PositionType::ptPlotCoords);
    targetLineCH4->start->setCoords(0, pidChannel[3]->target.toDouble());
    targetLineCH4->end->setType(QCPItemPosition::PositionType::ptPlotCoords);
    targetLineCH4->end->setCoords(10000, pidChannel[3]->target.toDouble());
    targetLineCH4->setVisible(false);
    connect(ui->pidTargetCH4, &QCheckBox::clicked, this, [=]()
    {
        if (ui->pidTargetCH4->checkState() == Qt::Checked)
        {
            targetLineCH4->start->setCoords(0, pidChannel[3]->target.toDouble());
            targetLineCH4->end->setCoords(10000, pidChannel[3]->target.toDouble());
            targetLineCH4->setVisible(true);
        }
        else
        {
            targetLineCH4->setVisible(false);
        }
        ui->customPlot->replot();
    });

    ui->customPlot->replot();                    //刷新曲线
}


/**
 * @brief PID界面控件是否可用
 */
void Widget::setPidUiIsEnabled(bool state)
{
    ui->pidPortName->setEnabled(state);
    ui->pidBaudRate->setEnabled(state);
    ui->pidCheckBit->setEnabled(state);
    ui->pidDataBit->setEnabled(state);
    ui->pidStopBit->setEnabled(state);

    //发送区与串口配置区相反
    ui->pidStateBtn->setChecked(!state);
    ui->pidSendParamBtn->setDisabled(state);
    ui->pidSendTargetBtn->setDisabled(state);
    ui->pidSendCycleBtn->setDisabled(state);
    ui->pidStartBtn->setDisabled(state);
    ui->pidResetBtn->setDisabled(state);
    ui->pidExportBtn->setDisabled(state);
    ui->pidClearDisplayBtn->setDisabled(state);
}

/**
 * @brief PID线程数据处理
 * @param msg 收的到的消息
 */
void Widget::pidMsgHandle(const COMMON_MSG::MSG& msg)
{
    if (msg == COMMON_MSG::MSG::ReadyRead) //接收到数据
    {
        QStringList strList = pidThread->recBuffer.dequeue();  //获取接收缓冲区的数据;
        switch (strList[0].toUShort())
        {
        case PID_SLAVE::Target:     //同步目标值
            ui->pidTargetEdit->setText(strList[2]);
            break;
        case PID_SLAVE::Actual:     //同步实际值
            if (ui->pidStopDisplayBtn->checkState() != Qt::Checked) //停止显示按钮没有被按下
            {
                pidCycleKey += pidCycleKeyStep;
                ui->customPlot->graph(strList[1].toInt() - 1)->addData(pidCycleKey, strList[2].toDouble());//添加数据到对应曲线

                //自动设定y轴的范围，如果不设定，有可能看不到图像
                //也可以用ui->customPlot->yAxis->setRange(up,low)手动设定y轴范围
                ui->customPlot->graph(0)->rescaleValueAxis();
                ui->customPlot->graph(1)->rescaleValueAxis(true);
                ui->customPlot->graph(2)->rescaleValueAxis(true);
                ui->customPlot->graph(3)->rescaleValueAxis(true);

                //自动设定y轴的范围，如果不设定，有可能看不到图像
                //也可以用ui->customPlot->yAxis->setRange(up,low)手动设定y轴范围
                if (pidCycleKey > 3)
                    ui->customPlot->xAxis->setRange(pidCycleKey + 2, 5, Qt::AlignRight);   //设定x轴的范围实现自动滚动
                ui->customPlot->replot();   //刷新图行
            }
            break;
        case PID_SLAVE::Param:      //同步PID参数
            ui->pidPEdit->setText(strList[2]);
            ui->pidIEdit->setText(strList[3]);
            ui->pidDEdit->setText(strList[4]);
            break;
        case PID_SLAVE::Start:      //同步启动按钮
            isPidStart = true;
            ui->pidStartBtn->click();
            break;
        case PID_SLAVE::Stop:       //同步停止按钮
            isPidStart = false;
            ui->pidStartBtn->click();
            break;
        case PID_SLAVE::Cycle:      //同步PID执行周期
            if (strList[2].toDouble() > 0.0)
            {
                ui->pidCycleEdit->setText(strList[2]);
                pidCycleKeyStep = strList[2].toDouble() / 1000.0;
            }
            break;
        default :
            break;
        }
    }
    else if (msg == COMMON_MSG::MSG::OpenFail) //打开失败
    {
        QMessageBox::warning(this, tr("警告"), tr("无法打开串口"));
        isPidConnected = false;
    }
    else if (msg == COMMON_MSG::MSG::OpenSuccessful) //打开成功
    {
        setPidUiIsEnabled(false);                 //改变控件的状态
        ui->pidOperateBtn->setText(tr("关闭串口"));   //改变按钮文字
        pidThread->start();
        isPidConnected = true;
    }

}


/**
 * @brief PID发送数据处理
 * @param cmd 命令
 */
void Widget::pidSendData(const PID_MASTER::CMD cmd)
{
    QByteArray sendText;
    sendText += QString("(") + QString::number(ui->pidChannelBtn->currentIndex() + 1) + QString("|");
    switch (cmd)
    {
    case PID_MASTER::Target:
        sendText += QString::number(PID_MASTER::Target) + QString("|");
        sendText += ui->pidTargetEdit->text() + QString("|");
        break;
    case PID_MASTER::Reset:
        sendText += QString::number((quint8)PID_MASTER::Reset) + QString("|");
        break;
    case PID_MASTER::Param:
        sendText += QString::number(PID_MASTER::Param) + QString("|");
        sendText += QString::asprintf("%f", ui->pidPEdit->text().toFloat()) + QString("|");
        sendText += QString::asprintf("%f", ui->pidIEdit->text().toFloat()) + QString("|");
        sendText += QString::asprintf("%f", ui->pidDEdit->text().toFloat()) + QString("|");

#ifndef QT_NO_DEBUG_OUTPUT
        qDebug() << "pidPEdit" << atof(QString::asprintf("%f", ui->pidPEdit->text().toFloat()).toStdString().data());
        qDebug() << "pidIEdit" << atof(QString::asprintf("%f", ui->pidIEdit->text().toFloat()).toStdString().data());
        qDebug() << "pidDEdit" << atof(QString::asprintf("%f", ui->pidDEdit->text().toFloat()).toStdString().data());
#endif
        break;
    case PID_MASTER::Start:
        sendText += QString::number(PID_MASTER::Start) + QString("|");
        break;
    case PID_MASTER::Stop:
        sendText += QString::number(PID_MASTER::Stop) + QString("|");
        break;
    case PID_MASTER::Cycle:
        sendText += QString::number(PID_MASTER::Cycle) + QString("|");
        sendText += ui->pidCycleEdit->text() + QString("|");
        break;
    default :
        break;
    }
    if (sendText.size() + 2 >= 10)
    {
        sendText += QString::number(sendText.size() + 3) + QString(")");
    }
    else
    {
        sendText += QString::number(sendText.size() + 2) + QString(")");
    }
    qDebug() << "sendText_size=" << sendText.size();
    emit sendPidDateSignal(sendText);
}


/**
 * @brief 网络通信初始化
 */
void Widget::netInit()
{
    /**********网络通信界面连上TCP服务器的客户端(左下侧界面)************/
    netTcpGridLayout = new QGridLayout();
    netTcpConClient = new QLabel();
    netTcpDisBtn = new QPushButton();
    netTcpDisBtn->setText(tr("断开"));
    netTcpClient = new QComboBox();
    netTcpClient->addItem(tr("All Client"));
    netTcpConClient->setText(tr("客户端"));
    addTcpServer(true);

    /***********网络通信界面UDP通信对方IP与端口(左下侧界面)************/
    netUdpGridLayout = new QGridLayout();
    netUdpDesLabel = new QLabel();
    netUdpDesLabel->setText(tr("目的地址:"));
    netUdpDesIp = new QLineEdit();
    netUdpDesPortLabel = new QLabel();
    netUdpDesPortLabel->setText(tr("目的端口:"));
    netUdpDesPort = new QLineEdit();

    /*************限制输入框内容**************************/
    QRegExp regExp(R"(^([1-9][0-9]{1,5})?$)");  //正则表达式(注意原始字面量)
    ui->netSendCycleEdit->setValidator(new QRegExpValidator(regExp, this));

    ui->netIp->setEditable(true);                        //使下拉选框可以编辑
    ui->netIp->addItems(Network::getAllLocalIp());       //更新可用IP

    /*************网络通信界面UI控件信号与槽连接**********************/
    connect(ui->netProtocol, &QComboBox::currentTextChanged, this, [=]()
    {
        static quint8 netIndex = 0;
        switch (ui->netProtocol->currentIndex())
        {
        case 0:
            ui->netOperateBtn->setText(tr("开始监听"));
            if (netIndex == 2)
            {
                addUdp(false);
            }
            addTcpServer(true);
            netIndex = 0;
            break;
        case 1:
            ui->netOperateBtn->setText(tr("开始连接"));
            if (netIndex == 0)
            {
                addTcpServer(false);
            }
            else
            {
                addUdp(false);
            }
            netIndex = 1;
            break;
        case 2:
            ui->netOperateBtn->setText(tr("打开"));
            if (netIndex == 0)
            {
                addTcpServer(false);
            }
            addUdp(true);
            netIndex = 2;
            break;
        default:
            break;
        }
    });
    connect(ui->netOperateBtn, &QPushButton::clicked, this, [=]()       //网络通信启动按钮
    {
        if (ui->netProtocol->isEnabled())
        {
            switch (ui->netProtocol->currentIndex())
            {
            case 0:
                ui->netOperateBtn->setText(tr("停止监听"));
                break;
            case 1:
                ui->netOperateBtn->setText(tr("断开连接"));
                break;
            case 2:
                ui->netOperateBtn->setText(tr("关闭"));
                break;
            }
            netConfig.clear();
            netConfig.append(QString::number(ui->netProtocol->currentIndex())); //通信类型
            netConfig.append(ui->netIp->currentText());    //本地IP(三种通信)
            netConfig.append(ui->netPort->text());         //本地端口(三种通信)

            /************通信配置正确性验证******************/
            if (ui->netProtocol->currentIndex() == 2)      //UDP通信
            {
                netConfig.append(netUdpDesIp->text());     //UDP远程IP
                udpClientIp = QHostAddress(netConfig[3]);
                if (udpClientIp.isNull())
                {
                    QMessageBox::warning(this, tr("错误"), tr("UDP远程IP填写错误"));
                    return;
                }
                netConfig.append(netUdpDesPort->text());     //UDP远程端口
                if (ui->netPort->text().toUInt() == 0 && netUdpDesPort->text().toUInt() == 0)
                {
                    QMessageBox::warning(this, tr("错误"), tr("端口填写错误"));
                    return;
                }
            }
            else if (ui->netPort->text().toUInt() == 0) //TCP通信端口
            {
                QMessageBox::warning(this, tr("错误"), tr("本地端口填写错误"));
                return;
            }

            emit startNetThread(netConfig); // NOLINT(readability-misleading-indentation)
            setNetConfigUiIsEnabled(false);
        }
        else  //停止监听或断开连接
        {
            netConnectFlag = false;
            emit closeNetThread();
            netThread->quit();
            netThread->wait();
            setNetConfigUiIsEnabled(true);
            netConnectUiIsEnabled(false);
            switch (ui->netProtocol->currentIndex())
            {
            case 0:
                ui->netOperateBtn->setText(tr("开始监听"));
                break;
            case 1:
                ui->netOperateBtn->setText(tr("开始连接"));
                break;
            case 2:
                ui->netOperateBtn->setText(tr("打开"));
                break;
            }
        }
    });
    connect(ui->netRecHexBtn, &QRadioButton::clicked, this, [=]()       //网络通信接收区16进制显示
    {
        if (!netHexFlag)
        {
            netHexFlag = true;
            QByteArray text = ui->netRecText->toPlainText().toUtf8(); //获取已经显示的文本
            QByteArray_to_HexQByteArray(text);
            ui->netRecText->setText(text);
        }
    });
    connect(ui->netRecAsciiBtn, &QPushButton::clicked, this, [=]()      //网络信接收区字符显示
    {
        if (netHexFlag)
        {
            netHexFlag = false;
            QString text = ui->netRecText->toPlainText(); //获取已经显示的文本
            HexQString_to_QString(text);
            ui->netRecText->setText(text);
        }
    });
    connect(ui->netRecSaveBtn, &QPushButton::clicked, this, [=]()   //接收区保存到文件
    {
        QString date = ui->netRecText->toPlainText();
        if (date.size() == 0)
        {
            QMessageBox::warning(this, tr("错误"), tr("接收区没有数据"));
            return;
        }
        QString path = QFileDialog::getSaveFileName(this, tr("保存文件"), QDir::home().path(), "Text files (*.txt)");
        if (path.size() == 0)
        {
            return;
        }
        QFile file(path);               //参数就是文件的路径
        file.open(QIODevice::WriteOnly | QIODevice::Text); //设置打开方式
        QTextStream text(&file);
        text << date;
        file.close();                   //关闭文件对象
    });
    connect(ui->netRecClearBtn, &QPushButton::clicked, this, [=]()      //网络通信接收区清空
    {
        ui->netRecText->clear();
    });
    connect(ui->netSendFileBtn, &QPushButton::clicked, this, [=]()      //选择文件发送
    {
        QString path = QFileDialog::getOpenFileName(this, "选择文件", QDir::home().path(), "Text files (*.txt)");
        if (path.size() == 0)
        {
            return;
        }
        QFile file(path);                   //参数就是文件的路径
        file.open(QIODevice::ReadOnly);     //设置打开方式
        QByteArray array = file.readAll();  //用QByteArray类去接收读取的信息
        ui->netSendText->setText(array);    //将读取到的数据放入textEdit中
        file.close();                       //关闭文件对象
    });
    connect(ui->netSendClearBtn, &QPushButton::clicked, this, [=]()     //发送去清除
    {
        ui->netSendText->clear();
    });
    connect(ui->netSendBtn, &QPushButton::clicked, this, &Widget::netSendDate);


    /***********网络通信定时自动发送数据********************************/
    netAutoSendTimer = new QTimer(this);
    connect(ui->netSendAutoBtn, &QCheckBox::stateChanged, this, [=]()
    {
        if (ui->netSendAutoBtn->isChecked())
        {
            qint32 time = ui->netSendCycleEdit->text().toInt();
            if (time != 0 && netConnectFlag)
            {
                netAutoSendTimer->start(time);
            }
        }
        else
        {
            netAutoSendTimer->stop();
        }
    });
    netSendCycle = ui->netSendCycleEdit->text();
    connect(ui->netSendCycleEdit, &QLineEdit::editingFinished, this, [=]()
    {
        if (ui->netSendCycleEdit->text().isNull())
        {
            ui->netSendCycleEdit->setText(netSendCycle);
        }
        netSendCycle = ui->netSendCycleEdit->text();
        if (ui->netSendAutoBtn->isChecked() && netConnectFlag)
        {
            qint32 time = ui->netSendCycleEdit->text().toInt();
            if (time != 0)
            {
                netAutoSendTimer->start(time);
            }
        }
    });
    connect(netAutoSendTimer, &QTimer::timeout, this, [=]()
    {
        if (netConnectFlag)
        {
            netSendDate();
        }
    });
}

/**
 * @brief 是否显示连上TCP服务器的客户端控件
 * @param state 控件显示状态
 *      @arg true 显示状态
 *      @arg false 不显示状态
 */
void Widget::addTcpServer(bool state)
{
    if (state)
    {
        netTcpConClient->setParent(ui->netConfigWidget);
        netTcpDisBtn->setParent(ui->netConfigWidget);
        netTcpClient->setParent(ui->netConfigWidget);
        netTcpGridLayout->addWidget(netTcpConClient, 0, 0, 1, 1);
        netTcpGridLayout->addWidget(netTcpDisBtn, 0, 1, 1, 1);
        netTcpGridLayout->addWidget(netTcpClient, 1, 0, 1, 2);

        ui->verticalLayout_3->addLayout(netTcpGridLayout);
    }
    else
    {
        netTcpConClient->setParent(nullptr);
        netTcpDisBtn->setParent(nullptr);
        netTcpClient->setParent(nullptr);
        ui->verticalLayout_3->removeItem(netTcpGridLayout);
    }
}

/**
 * @brief 是否显示对方UDP信息控件
 * @param state 控件显示状态
 *      @arg true 显示状态
 *      @arg false 不显示状态
 */
void Widget::addUdp(bool state)
{
    if (state)
    {
        netUdpDesLabel->setParent(ui->netConfigWidget);
        netUdpDesIp->setParent(ui->netConfigWidget);
        netUdpDesPortLabel->setParent(ui->netConfigWidget);
        netUdpDesPort->setParent(ui->netConfigWidget);
        netUdpGridLayout->addWidget(netUdpDesLabel, 0, 0, 1, 1);
        netUdpGridLayout->addWidget(netUdpDesIp, 0, 1, 1, 1);
        netUdpGridLayout->addWidget(netUdpDesPortLabel, 1, 0, 1, 1);
        netUdpGridLayout->addWidget(netUdpDesPort, 1, 1, 1, 1);
        ui->verticalLayout_3->addLayout(netUdpGridLayout);
    }
    else
    {
        netUdpDesLabel->setParent(nullptr);
        netUdpDesIp->setParent(nullptr);
        netUdpDesPortLabel->setParent(nullptr);
        netUdpDesPort->setParent(nullptr);
        ui->verticalLayout_3->removeItem(netUdpGridLayout);
    }
}

/**
 * @brief 网络通信界面UI控件是否可用
 * @param state 状态
 */
void Widget::setNetConfigUiIsEnabled(bool state)
{
    ui->netProtocol->setEnabled(state);
    ui->netIp->setEnabled(state);
    ui->netPort->setEnabled(state);
}

/**
 * @brief 网络通信控件空间变化
 * @param state 状态
 */
void Widget::netConnectUiIsEnabled(bool state)
{
    ui->netStateBtn->setChecked(state);
    ui->netSendBtn->setEnabled(state);
}

/**
 * @brief 处理网络通信消息
 * @param msg 收到的消息
 */
void Widget::netMsgHandle(const COMMON_MSG::MSG& msg)
{
    if (msg == COMMON_MSG::MSG::ReadyRead)  //可以读到数据
    {
        if (widgetIndex == 2) //网络数据显示页面
        {
            if (ui->netRecStopBtn->checkState() != Qt::Checked) //暂停接收按钮没有被按下
            {
                QByteArray recText = netThread->recBuffer.dequeue();         //获取接收缓冲区的数据
                recBytes += recText.length();                                 //追加接收到的字符字节数量
                ui->recLabel->setText(QString("接收字节数:%1").arg(recBytes));
                QString lastText = ui->netRecText->toPlainText(); //获取已经显示的文本
                if (ui->netRecHexBtn->isChecked())                //判断是否显示16进制
                {
                    QByteArray_to_HexQByteArray(recText);
                }
                lastText += recText;                               //在末尾追加字符串
                ui->netRecText->setText(lastText);                 //刷新文本显示
                ui->netRecText->moveCursor(QTextCursor::End);      //滑动显示的文本框
            }
            else
            {
                netThread->recBuffer.clear();
            }
        }
        else if (widgetIndex == 3)   //摄像头显示界面
        {

        }
    }
    else if (msg == COMMON_MSG::MSG::Connected)     //连接成功
    {
        netConnectFlag = true;
        netThread->start();
        netConnectUiIsEnabled(true);
    }
    else if (msg == COMMON_MSG::MSG::Disconnected)     //断开连接
    {
        netAutoSendTimer->stop();
        netConnectFlag = false;
        netConnectUiIsEnabled(false);
        emit closeNetThread();
        netThread->quit();
        netThread->wait();
    }
}

/**
 * @brief 网络线程发送数据
 */
void Widget::netSendDate()
{
    QByteArray sendText;
    sendText = ui->netSendText->toPlainText().toUtf8();

    if (ui->netSendNewlineBtn->checkState() == Qt::Checked)     //发送新行被选中
        sendText += '\n';

    if (ui->netSendHexBtn->isChecked())                         //16进制发送
        sendText = sendText.toHex();

    emit sendNetDateSignal(sendText);
    sendBytes += sendText.length();                            //累加发送字符字节数
    ui->sendLabel->setText(QString("发送字节数:%1").arg(sendBytes));
}

/**
 * @brief ui界面初始化
 */
void Widget::oscUiInit()
{
    /**********网络通信界面连上TCP服务器的客户端(左下侧界面)************/
    oscTcpGridLayout = new QGridLayout();
    oscTcpConClient = new QLabel();
    oscTcpDisBtn = new QPushButton();
    oscTcpDisBtn->setText(tr("断开"));
    oscTcpClient = new QComboBox();
    oscTcpClient->addItem(tr("All Client"));
    oscTcpConClient->setText(tr("客户端"));
    oscAddTcpServer(true);

    /***********网络通信界面UDP通信对方IP与端口(左下侧界面)************/
    oscUdpGridLayout = new QGridLayout();
    oscUdpDesLabel = new QLabel();
    oscUdpDesLabel->setText(tr("目的地址:"));
    oscUdpDesIp = new QLineEdit();
    oscUdpDesPortLabel = new QLabel();
    oscUdpDesPortLabel->setText(tr("目的端口:"));
    oscUdpDesPort = new QLineEdit();


    ui->oscIp->setEditable(true);                  // 使下拉选框可以编辑
    ui->oscIp->addItems(OSC::getAllLocalIp()); // 更新可用IP

    /*************网络通信界面UI控件信号与槽连接**********************/
    connect(ui->oscProtocol, &QComboBox::currentTextChanged, this, [=]()
    {
        static quint8 oscIndex = 0;
        qDebug() << oscIndex;
        switch (ui->oscProtocol->currentIndex())
        {
        case 0:
            ui->oscOperateBtn->setText(tr("开始监听"));
            if (oscIndex == 2)
            {
                oscAddUdp(false);
            }
            oscAddTcpServer(true);
            oscIndex = 0;
            break;
        case 1:
            ui->oscOperateBtn->setText(tr("开始连接"));
            if (oscIndex == 0)
            {
                oscAddTcpServer(false);
            }
            else
            {
                oscAddUdp(false);
            }
            oscIndex = 1;
            break;
        case 2:
            ui->oscOperateBtn->setText(tr("打开"));
            if (oscIndex == 0)
            {
                oscAddTcpServer(false);
            }
            oscAddUdp(true);
            oscIndex = 2;
            break;
        default:
            break;
        }
    });
    // 网络通信启动按钮
    connect(ui->oscOperateBtn, &QPushButton::clicked, this, [=]()
    {
        if (ui->oscProtocol->isEnabled())
        {
            switch (ui->oscProtocol->currentIndex())
            {
            case 0:
                ui->oscOperateBtn->setText(tr("停止监听"));
                break;
            case 1:
                ui->oscOperateBtn->setText(tr("断开连接"));
                break;
            case 2:
                ui->oscOperateBtn->setText(tr("关闭"));
                break;
            }
            oscConfig.clear();
            oscConfig.append(QString::number(ui->oscProtocol->currentIndex())); //通信类型
            oscConfig.append(ui->oscIp->currentText());    //本地IP(三种通信)
            oscConfig.append(ui->oscPort->text());         //本地端口(三种通信)

            /************通信配置正确性验证******************/
            if (ui->oscProtocol->currentIndex() == 2)      //UDP通信
            {
                oscConfig.append(oscUdpDesIp->text());     //UDP远程IP
                oscUdpClientIp = QHostAddress(oscConfig[3]);
                if (oscUdpClientIp.isNull())
                {
                    QMessageBox::warning(this, tr("错误"), tr("UDP远程IP填写错误"));
                    return;
                }
                oscConfig.append(oscUdpDesPort->text());     //UDP远程端口
                if (ui->oscPort->text().toUInt() == 0 && oscUdpDesPort->text().toUInt() == 0)
                {
                    QMessageBox::warning(this, tr("错误"), tr("端口填写错误"));
                    return;
                }
            }
            else if (ui->oscPort->text().toUInt() == 0) //TCP通信端口
            {
                QMessageBox::warning(this, tr("错误"), tr("本地端口填写错误"));
                return;
            }

            emit startOscThread(oscConfig); // NOLINT(readability-misleading-indentation)
            setOscConfigUiIsEnabled(false);
        }
        else  //停止监听或断开连接
        {
            oscConnectFlag = false;
            emit closeOscThread();
            oscThread->quit();
            oscThread->wait();
            setOscConfigUiIsEnabled(true);
            oscConnectUiIsEnabled(false);
            switch (ui->oscProtocol->currentIndex())
            {
            case 0:
                ui->oscOperateBtn->setText(tr("开始监听"));
                break;
            case 1:
                ui->oscOperateBtn->setText(tr("开始连接"));
                break;
            case 2:
                ui->oscOperateBtn->setText(tr("打开"));
                break;
            }
        }
    });
    connect(ui->oscSendBtn, &QPushButton::clicked, this, &Widget::oscSendDate);

    /****************曲线处理********************************/
    ui->oscCustomPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iMultiSelect); // 可拖拽+可滚轮缩放+可以被选中


    //背景网格样式
    ui->oscCustomPlot->xAxis->grid()->setPen(QPen(QColor(50, 50, 50), 1, Qt::SolidLine));
    ui->oscCustomPlot->yAxis->grid()->setPen(QPen(Qt::SolidLine));
    ui->oscCustomPlot->yAxis->grid()->setZeroLinePen(QPen(Qt::yellow, 1));    //y轴0刻度颜色
    ui->oscCustomPlot->xAxis->grid()->setSubGridPen(QPen(QColor(50, 50, 50), 1, Qt::DotLine));//子网格浅色点线
    ui->oscCustomPlot->yAxis->grid()->setSubGridPen(QPen(QColor(50, 50, 50), 1, Qt::DotLine));//子网格浅色点线
    ui->oscCustomPlot->xAxis->grid()->setSubGridVisible(true);//显示x轴子网格线
    ui->oscCustomPlot->yAxis->grid()->setSubGridVisible(true);//显示要轴子网格线
    // ui->oscCustomPlot->xAxis->setTickLabels(false);    //横坐标值不可见


    // customplot->xAxis->ticker()->setTickOrigin(1);//改变刻度原点为1

    ui->oscCustomPlot->xAxis->setRange(0, 940, Qt::AlignLeft); // 设定x轴的范围
    ui->oscCustomPlot->yAxis->setRange(-210, 210);  // 设定y轴的范围

    /****************添加两条曲线*****************************/
    ui->oscCustomPlot->addGraph();                         // 添加一条曲线
    ui->oscCustomPlot->graph(0)->setPen(QPen(Qt::red));    // 曲线红色
    ui->oscCustomPlot->graph(0)->setName("CH1");           // 图例名称
    ui->oscCustomPlot->graph(0)->setVisible(true);         // 曲线有效显示
    ui->oscCustomPlot->addGraph();
    ui->oscCustomPlot->graph(1)->setPen(QPen(Qt::blue));
    ui->oscCustomPlot->graph(1)->setName("CH2");
    ui->oscCustomPlot->graph(1)->setVisible(true);

    /*************曲线图例设置****************************/
    ui->oscCustomPlot->legend->setFillOrder(QCPLayoutGrid::foRowsFirst);       // 设置图例优先按照列放置
    ui->oscCustomPlot->legend->setWrap(4);                                     // 优先规则中最大数量(一排最多数量或一列最多数量，取决于优先放置规则)
    ui->oscCustomPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignRight | Qt::AlignTop);  //图例放置在右上角
    ui->oscCustomPlot->legend->setBorderPen(Qt::NoPen);                        // 设置边框隐藏
    ui->oscCustomPlot->legend->setVisible(true);                               // 图例可见
    ui->oscCustomPlot->replot(); // 刷新曲线
}

/**
 * @brief 是否显示连上TCP服务器的客户端控件
 * @param state 控件显示状态
 *      @arg true 显示状态
 *      @arg false 不显示状态
 */
void Widget::oscAddTcpServer(bool state)
{
    if (state)
    {
        oscTcpConClient->setParent(ui->oscConfigWidget);
        oscTcpDisBtn->setParent(ui->oscConfigWidget);
        oscTcpClient->setParent(ui->oscConfigWidget);
        oscTcpGridLayout->addWidget(oscTcpConClient, 0, 0, 1, 1);
        oscTcpGridLayout->addWidget(oscTcpDisBtn, 0, 1, 1, 1);
        oscTcpGridLayout->addWidget(oscTcpClient, 1, 0, 1, 2);
        ui->verticalLayout_8->addLayout(oscTcpGridLayout);
    }
    else
    {
        oscTcpConClient->setParent(nullptr);
        oscTcpDisBtn->setParent(nullptr);
        oscTcpClient->setParent(nullptr);
        ui->verticalLayout_8->removeItem(oscTcpGridLayout);
    }
}

/**
 * @brief 是否显示对方UDP信息控件
 * @param state 控件显示状态
 *      @arg true 显示状态
 *      @arg false 不显示状态
 */
void Widget::oscAddUdp(bool state)
{
    if (state)
    {
        oscUdpDesLabel->setParent(ui->oscConfigWidget);
        oscUdpDesIp->setParent(ui->oscConfigWidget);
        oscUdpDesPortLabel->setParent(ui->oscConfigWidget);
        oscUdpDesPort->setParent(ui->oscConfigWidget);
        oscUdpGridLayout->addWidget(oscUdpDesLabel, 0, 0, 1, 1);
        oscUdpGridLayout->addWidget(oscUdpDesIp, 0, 1, 1, 1);
        oscUdpGridLayout->addWidget(oscUdpDesPortLabel, 1, 0, 1, 1);
        oscUdpGridLayout->addWidget(oscUdpDesPort, 1, 1, 1, 1);
        ui->verticalLayout_8->addLayout(oscUdpGridLayout);
    }
    else
    {
        oscUdpDesLabel->setParent(nullptr);
        oscUdpDesIp->setParent(nullptr);
        oscUdpDesPortLabel->setParent(nullptr);
        oscUdpDesPort->setParent(nullptr);
        ui->verticalLayout_8->removeItem(oscUdpGridLayout);
    }
}

/**
 * @brief 网络通信界面UI控件是否可用
 * @param state 状态
 */
void Widget::setOscConfigUiIsEnabled(bool state)
{
    ui->oscProtocol->setEnabled(state);
    ui->oscIp->setEnabled(state);
    ui->oscPort->setEnabled(state);
}

/**
 * @brief 网络通信控件空间变化
 * @param state 状态
 */
void Widget::oscConnectUiIsEnabled(bool state)
{
    ui->oscStateBtn->setChecked(state);
    ui->oscSendBtn->setEnabled(state);
}

/**
 * @brief 处理网络通信消息
 * @param msg 收到的消息
 */
void Widget::oscMsgHandle(const COMMON_MSG::MSG& msg)
{
    if (msg == COMMON_MSG::MSG::ReadyRead) // 可以读到数据
    {
        if (ui->oscRecStopBtn->checkState() != Qt::Checked) // 暂停接收按钮没有被按下
        {
            QByteArray recText = oscThread->recBuffer.dequeue(); // 获取接收缓冲区的数据
            QVector<double> xData, yData;

            qDebug() << "size" << recText.size();
            qDebug() << recText;
            for (int i = 0; i < recText.size(); i++)
            {
                xData.append(i);
                yData.append(static_cast<quint8>(recText[i]));  //只能获取字符型，需要转换
            }
            qDebug() << yData;
            qDebug() << "yData[0]" << yData[0];
            ui->oscCustomPlot->graph(0)->setData(xData, yData); // 添加数据到对应曲线（带清空数据）

            // // 自动设定y轴的范围，如果不设定，有可能看不到图像
            // // 也可以用ui->oscCustomPlot->yAxis->setRange(up,low)手动设定y轴范围

            ui->oscCustomPlot->replot();                // 刷新图形

            ui->recLabel->setText(QString("接收字节数:%1").arg(recText.size()));
        }
        else
        {
            oscThread->recBuffer.clear();
        }
    }
    else if (msg == COMMON_MSG::MSG::Connected) // 连接成功
    {
        oscConnectFlag = true;
        oscThread->start();
        oscConnectUiIsEnabled(true);
    }
    else if (msg == COMMON_MSG::MSG::Disconnected) // 断开连接
    {
        oscConnectFlag = false;
        oscConnectUiIsEnabled(false);
        emit closeOscThread();
        oscThread->quit();
        oscThread->wait();
    }
}

/**
 * @brief 网络线程发送数据
 */
void Widget::oscSendDate()
{
    QByteArray sendText;
    sendText = ui->oscSendText->toPlainText().toUtf8();

    // if (ui->netSendNewlineBtn->checkState() == Qt::Checked) // 发送新行被选中
    //     sendText += '\n';

    // if (ui->netSendHexBtn->isChecked()) // 16进制发送
    //     sendText = sendText.toHex();


    // uint16_t N = 512; // 采样点数
    // float sample_freq     = 120.0f;                     // 采样频率 120 Hz, 大于两倍的最高频率
    // float sample_interval = 1 / sample_freq;            // 采样间隔0.008333步长
    // float t               = 0;

    // for (int i = 0; i < N; ++i)
    // {
    //     t += sample_interval;
    //     sendText.append((uint8_t)(100 * (sin(2 * M_PI * t) + 1)) + 1); //加1保证数据不为0，否者上位机认为无效数据
    // }

    emit sendOscDateSignal(sendText);
    sendBytes += sendText.length(); // 累加发送字符字节数
    ui->sendLabel->setText(QString("发送字节数:%1").arg(sendBytes));
}

/**
 * @brief 下载界面初始化
 */
void Widget::downloadInit()
{
    connect(ui->dowFileBtn, &QToolButton::clicked, this, [=]()      //选择文件按钮
    {
        QString path = QFileDialog::getOpenFileName(this, tr("选择文件"), QDir::home().path(), "Flash file (*.bin)");
        if (!path.isNull())
        {
            ui->dowFile->setText(path);
        }
    });
    connect(ui->dowClearLogBtn, &QPushButton::clicked, this, [=]()  //清除接收日志文本
    {
        ui->dowLogText->clear();
    });
    connect(ui->dowClearSendBtn, &QPushButton::clicked, this, [=]() //清空发送文本
    {
        ui->dowSendText->clear();
    });

    //置零下载进度
    ui->dowProgressBar->reset();


    /**************ISP界面*****************************/
    ui->ispDowBaudRate->setCurrentIndex(5);
    ui->ispDowHWBtn->setCurrentIndex(3);
    auto isp_ports = SerialPortInfo::availablePorts();
    ui->ispDowPortName->clear();
    ui->ispDowPortName->addItems(isp_ports);
    ui->ispDowReadInfoBtn->setEnabled(false);
    ui->ispDowFlashBtn->setEnabled(false);
    ui->ispDowEraseBtn->setEnabled(false);
    connect(serialPortInfo, &SerialPortInfo::update, this, [=](const QStringList& ports) //刷新可用串口
    {
        ui->ispDowPortName->clear();
        ui->ispDowPortName->addItems(ports);
    });

    /*****************ISP界面按钮信号与槽连接*************************************/
    connect(ui->ispDowOpBtn, &QPushButton::clicked, this, [=]() //串口操作按钮
    {
        dowCmdArg.clear();
        if (!ispConnected)
        {
            dowCmdArg.append(ui->ispDowPortName->currentText());  //端口名
            dowCmdArg.append(ui->ispDowBaudRate->currentText());  //波特率
            dowCmdArg.append(QString::number(ui->ispDowHWBtn->currentIndex()));    //流控
            this->dowPackCmd(DOW::TYPE::ISP, DOW::Open, dowCmdArg);
        }
        else
        {
            ispConnected = false;
            ui->ispDowOpBtn->setText(tr("打开串口"));
            ui->ispDowReadInfoBtn->setEnabled(false);
            ui->ispDowFlashBtn->setEnabled(false);
            ui->ispDowEraseBtn->setEnabled(false);

            this->dowPackCmd(DOW::TYPE::ISP, DOW::Close, dowCmdArg);
        }
    });
    connect(ui->ispDowFlashBtn, &QPushButton::clicked, this, [=]()     //烧录按钮
    {
        dowCmdArg.clear();
        dowCmdArg << ui->dowFile->text();
        this->dowPackCmd(DOW::TYPE::ISP, DOW::Flash, dowCmdArg);
    });
    connect(ui->ispDowReadInfoBtn, &QPushButton::clicked, this, [=]()  //读取芯片信息按钮
    {
        dowCmdArg.clear();
        this->dowPackCmd(DOW::TYPE::ISP, DOW::ReadInfo, dowCmdArg);
    });
    connect(ui->ispDowEraseBtn, &QPushButton::clicked, this, [=]()     //擦除按钮
    {
        dowCmdArg.clear();
        this->dowPackCmd(DOW::TYPE::ISP, DOW::Erase, dowCmdArg);
    });

    /**************IAP界面*****************************/
    ui->iapDowBaudRate->setCurrentIndex(6);
    auto iap_ports = SerialPortInfo::availablePorts();
    ui->iapDowPortName->clear();
    ui->iapDowPortName->addItems(iap_ports);
    ui->iapDowReadInfoBtn->setEnabled(false);
    ui->iapDowFlashBtn->setEnabled(false);
    ui->iapDowEraseBtn->setEnabled(false);
    connect(serialPortInfo, &SerialPortInfo::update, this, [=](const QStringList& ports) //刷新可用串口
    {
        ui->iapDowPortName->clear();
        ui->iapDowPortName->addItems(ports);
    });

    /*****************IAP界面按钮信号与槽连接*************************************/
    connect(ui->iapDowOpBtn, &QPushButton::clicked, this, [=]() //串口操作按钮
    {
        dowCmdArg.clear();
        if (!iapConnected)
        {
            dowCmdArg.append(ui->iapDowPortName->currentText());  //端口名
            dowCmdArg.append(ui->iapDowBaudRate->currentText());  //波特率
            this->dowPackCmd(DOW::TYPE::IAP, DOW::Open, dowCmdArg);
        }
        else
        {
            iapConnected = false;
            ui->iapDowOpBtn->setText(tr("打开串口"));
            ui->iapDowReadInfoBtn->setEnabled(false);
            ui->iapDowFlashBtn->setEnabled(false);
            ui->iapDowEraseBtn->setEnabled(false);
            this->dowPackCmd(DOW::TYPE::IAP, DOW::Close, dowCmdArg);
        }
    });
    connect(ui->iapDowFlashBtn, &QPushButton::clicked, this, [=]()     //烧录按钮
    {
        if (!ui->dowFile->text().isEmpty())
        {
            dowCmdArg.clear();
            dowCmdArg << ui->dowFile->text();
            this->dowPackCmd(DOW::TYPE::IAP, DOW::Flash, dowCmdArg);
        }
        else
        {
            QMessageBox::warning(this, QString(tr("警告")), QString(tr("无固件")));
        }
    });
    connect(ui->iapDowReadInfoBtn, &QPushButton::clicked, this, [=]()  //读取芯片信息按钮
    {
        dowCmdArg.clear();
        this->dowPackCmd(DOW::TYPE::IAP, DOW::ReadInfo, dowCmdArg);
    });
    connect(ui->iapDowEraseBtn, &QPushButton::clicked, this, [=]()     //擦除按钮
    {
        dowCmdArg.clear();
        this->dowPackCmd(DOW::TYPE::IAP, DOW::Erase, dowCmdArg);
    });
}

/**
 * @brief 发送ISP命令
 * @param cmd ISP命令
 */
void Widget::dowPackCmd(DOW::TYPE type, DOW::CMD cmd, QStringList& arg)
{
    QStringList config;
    config.append(QString("%1").arg(type));
    config.append(QString("%1").arg(cmd));
    config.append(arg);
    emit sendDowCmd(config);
}

/**
 * @brief 处理下载线程消息
 * @param msg 消息
 */
void Widget::dowMsgHandle(const DOW::TYPE type, const COMMON_MSG::MSG& msg)
{
    QByteArray text;
    switch (msg)
    {
    case COMMON_MSG::MSG::OpenFail:
        iapConnected = false;
        QMessageBox::warning(this, tr("警告"), tr("无法打开串口"));
        break;
    case COMMON_MSG::MSG::OpenSuccessful:
        if (type == DOW::TYPE::ISP)
        {
            ispConnected = true;
            ui->ispDowReadInfoBtn->setEnabled(true);
            ui->ispDowFlashBtn->setEnabled(true);
            ui->ispDowEraseBtn->setEnabled(true);
            ui->ispDowOpBtn->setText(tr("关闭串口"));
        }
        else if (type == DOW::TYPE::IAP)
        {
            iapConnected = true;
            ui->iapDowReadInfoBtn->setEnabled(true);
            ui->iapDowFlashBtn->setEnabled(true);
            ui->iapDowEraseBtn->setEnabled(true);
            ui->iapDowOpBtn->setText(tr("关闭串口"));
        }
        break;
    case COMMON_MSG::MSG::ReciveLog:
        ui->dowLogText->moveCursor(QTextCursor::End);  //移动光标到末尾
        text = dowThreadWork->recBuffer.dequeue();
        ui->dowLogText->insertPlainText(text);  //文末追加文本
        break;
    default:
        break;
    }

}

/**
 * @brief hex字符串转为ascii字符串
 * @param source 源文本
 * @return 转换后的文本
 */
void Widget::HexQString_to_QString(QString& source)
{
    QByteArray temp;
    temp = source.remove(" ").toLatin1();

    //将temp中的16进制字符串转化为对应的字符串，比如"31"转换为"1"
    source = QByteArray::fromHex(temp);
}

/**
 * @brief ascii字节流转hex字节流
 * @param source 源文本
 * @return 换后的文本
 */
void Widget::QByteArray_to_HexQByteArray(QByteArray& source)
{
    source = source.toHex();
    int length = source.length();
    for (int i = 0; i <= (length / 2) - 1; ++i)
    {
        source.insert((3 * i + 2), ' '); //插入空格字符串便于区分一个字节
    }
}


