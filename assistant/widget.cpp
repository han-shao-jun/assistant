#include "widget.h"
#include "ui_widget.h"

Widget::Widget(QWidget *parent) : QWidget(parent), ui(new Ui::Widget)
{
    ui->setupUi(this);

    qDebug() << "main id: " << QThread::currentThreadId();
    this->installEventFilter(this); // 安装事件过滤器

    /************注册元类型，用于信号与槽传递数据****************/
    qRegisterMetaType<COMMON_MSG::MSG>("COMMON_MSG::MSG");
    qRegisterMetaType<DOW::CMD>("DOW::CMD");
    qRegisterMetaType<DOW::TYPE>("DOW::TYPE");

    serialPortInfo = new SerialPortInfo(this);

    /**************新建通信线程*********************************/
    comThread = new Com();
    connect(this, &Widget::startComThread, comThread, &Com::recConfig); // 主线程启动通信线程
    connect(this, &Widget::closeComThread, comThread, &Com::close); // 主线程结束通信线程
    connect(this, &Widget::sendComDateSignal, comThread, &Com::write); // 主线程向通信信线程发送数据
    connect(comThread, &Com::msgSignal, this, &Widget::comMsgHandle); // 主线程接收通信线程数据

    /**********PID绘图4个通道********************************/
    pidChannel[0] = new PidParam_t;
    pidChannel[1] = new PidParam_t;
    pidChannel[2] = new PidParam_t;
    pidChannel[3] = new PidParam_t;
    pidChannel[0]->isDisplayActual = true;

    /**************新建PID页面线程*********************************/
    pidThread = new Pid();
    connect(this, &Widget::startPidThread, pidThread, &Pid::recConfig); // 主线程启动串口线程
    connect(this, &Widget::closePidThread, pidThread, &Pid::close); // 主线程结束串口线程
    connect(this, &Widget::sendPidDateSignal, pidThread, &Pid::write); // 主线程向串口线程发送数据
    connect(pidThread, &Pid::msgSignal, this, &Widget::pidMsgHandle); // 主线程接收串口线程数据

    /***********新建OSC页面线程**********************************/
    oscThread = new OSC();
    connect(this, &Widget::startOscThread, oscThread, &OSC::recConfig); // 主线程启动网络通信线程
    connect(this, &Widget::closeOscThread, oscThread, &OSC::close); // 主线程结束网络通信线程
    connect(this, &Widget::sendOscDateSignal, oscThread,
            &OSC::write); // 主线程向网络通信线程发送数据
    connect(oscThread, &OSC::msgSignal, this, &Widget::oscMsgHandle); // 主线程接收网络通信线程数据

    /***********新建下载页面线程**********************************/
    dowThread = new QThread();      // 下载子线程
    dowThreadWork = new Download(); // 工作类
    dowThreadWork->moveToThread(dowThread);
    connect(this, &Widget::sendDowCmd, dowThreadWork, &Download::doWork); // 主线程触发子线程
    connect(dowThreadWork, &Download::msgSignal, this,
            &Widget::dowMsgHandle); // 主线程接收下载线程消息
    dowThread->start();

    /***********初始化界面*******************/
    this->menuInit();
    this->comInit();
    this->uartInit();
    this->netInit();
    this->pidInit();
    this->oscUiInit();
    this->downloadInit();

    // 清除计数按钮
    connect(ui->clearCounterBtn, &QPushButton::clicked, this, [=]() {
        recBytes = 0;
        sendBytes = 0;
        ui->recLabel->setText(tr("接收字节数:0"));
        ui->sendLabel->setText(tr("发送字节数:0"));
    });
}

Widget::~Widget()
{
    if (isComConnected) {
        Q_EMIT closeComThread();
        comThread->quit();
        comThread->wait();
    }
    delete comThread;

    if (isPidConnected) {
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

    if (oscConnectFlag) {
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
    ui->mainWidget->setCurrentIndex(0);
    widgetIndex = ui->mainWidget->currentIndex();
    ui->menuComBtn->setStyleSheet("QPushButton{\n"
                                  "    border-style:none;\n"
                                  "    border-top: 1px solid #23A7F2;\n"
                                  "    color:#118DC0;\n"
                                  "    min-height: 25px;"
                                  "    background: #1E1E1E;\n"
                                  "}");
    connect(ui->mainWidget, &QStackedWidget::currentChanged, this, [=]() {
        QString Selected = "QPushButton{\n"
                           "    border-style:none;\n"
                           "    border: none;"
                           "    border-top: 1px solid #23A7F2;\n"
                           "    color:#23A7F2;\n"
                           "    min-height: 25px;"
                           "    background: #1E1E1E;\n"
                           "}";

        QString noSelected = "QPushButton{\n"
                             "    border-style:none;\n"
                             "    border: none;"
                             "    color:#DCDCDC;\n"
                             "    min-height: 25px;\n"
                             "    background-color: #4B4B4B;\n"
                             "}\n"
                             "\n"
                             "QPushButton:hover{\n"
                             "    border: none;"
                             "    border-style:none;"
                             "    color:#FFFFFF;\n"
                             "    background-color:rgba(51,127,209,230);\n"
                             "}";

        switch (widgetIndex) {
        case 0:
            ui->menuComBtn->setStyleSheet(noSelected);
            break;
        case 1:
            ui->menuPidBtn->setStyleSheet(noSelected);
            break;
        case 2:
            ui->menuOscBtn->setStyleSheet(noSelected);
            break;
        default:
            ui->menuDowBtn->setStyleSheet(noSelected);
            break;
        }
        widgetIndex = ui->mainWidget->currentIndex();
        switch (widgetIndex) {
        case 0:
            ui->menuComBtn->setStyleSheet(Selected);
            break;
        case 1:
            ui->menuPidBtn->setStyleSheet(Selected);
            break;
        case 2:
            ui->menuOscBtn->setStyleSheet(Selected);
            break;
        default:
            ui->menuDowBtn->setStyleSheet(Selected);
            break;
        }

        if (isComConnected) {
            ui->uartOperateBtn->click();
        }
        if (isPidConnected) {
            ui->pidOperateBtn->click();
        }
    });

    connect(ui->menuComBtn, &QPushButton::clicked, this, &Widget::menuBtnClicked);
    connect(ui->menuPidBtn, &QPushButton::clicked, this, &Widget::menuBtnClicked);
    connect(ui->menuOscBtn, &QPushButton::clicked, this, &Widget::menuBtnClicked);
    connect(ui->menuDowBtn, &QPushButton::clicked, this, &Widget::menuBtnClicked);
}

/**
 * @brief 菜单栏按钮槽函数
 */
void Widget::menuBtnClicked()
{
    QString objName = sender()->objectName(); // 获取发送信号对象的名字
    int index;
    if (objName == "menuComBtn") {
        index = 0;
    } else if (objName == "menuPidBtn") {
        index = 1;
    } else if (objName == "menuOscBtn") {
        index = 2;
    } else if (objName == "menuDowBtn") {
        index = 3;
    } else {
        return;
    }
    ui->mainWidget->setCurrentIndex(index);
}

/**
 * @brief 初始化通信界面初
 */
void Widget::comInit()
{
    ui->interfaceBtn->setView(new QListView(ui->interfaceBtn));
    ui->comRecLogCheck->setToolTip(tr("追加时间戳和换行"));

    // 数据接口选择按钮
    connect(ui->interfaceBtn, &QComboBox::currentIndexChanged, this, [=](int index) {
        ui->interfacePage->setCurrentIndex(index);
        if (isComConnected) {
            ui->uartOperateBtn->click();
        }
        if (isPidConnected) {
            ui->pidOperateBtn->click();
        }
    });

    // 接收区16进制显示按钮
    connect(ui->comRecHexBtn, &QRadioButton::clicked, this, [=]() {
        if (!comHexFlag) {
            comHexFlag = true;
            QByteArray text = ui->comRecText->toPlainText().toUtf8(); // 获取已经显示的文本
            if (!text.isEmpty()) {
                QByteArray_to_HexQByteArray(text);
                ui->comRecText->setText(text);
            }
        }
    });

    // 接收区字符显示按钮
    connect(ui->comRecAsciiBtn, &QPushButton::clicked, this, [=]() {
        if (comHexFlag) {
            comHexFlag = false;
            QString text = ui->comRecText->toPlainText(); // 获取已经显示的文本
            if (!text.isEmpty()) {
                HexQString_to_QString(text);
                ui->comRecText->setText(text);
            }
        }
    });

    // 保存到文件按钮
    connect(ui->comRecSaveBtn, &QPushButton::clicked, this, [=]() {
        QString data = ui->comRecText->toPlainText();
        if (data.isNull()) {
            QMessageBox::critical(this, tr("错误"), tr("接收区没有内容"));
            return;
        }
        QString fileName = QFileDialog::getSaveFileName(this, tr("保存文件"), QDir::home().path(),
                                                        "Text files (*.txt)");
        if (fileName.isNull()) {
            QMessageBox::critical(this, tr("错误"), tr("保存文件名不能为空"));
            return;
        }
        /*******保存文件**************/
        QFile file(fileName);
        file.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream text(&file);
        text << data;
        file.close();
    });

    // 清除接收区按钮
    connect(ui->comRecClearBtn, &QPushButton::clicked, this, [=]() { ui->comRecText->clear(); });

    // 发送区选择文件按钮
    connect(ui->comSendFileBtn, &QPushButton::clicked, this, [=]() {
        auto fileName = QFileDialog::getOpenFileName(this, tr("选择文件"), QDir::home().path(),
                                                     "Text files (*.txt)");
        if (fileName.size() == 0) {
            QMessageBox::critical(this, tr("错误"), tr("文件名不能为空"));
            return;
        }

        /****打开文件并读取****/
        QFile file(fileName);
        file.open(QIODevice::ReadOnly);
        QByteArray array = file.readAll();
        ui->comSendText->setText(array);
        file.close();
    });

    // 发送区清除按钮
    connect(ui->comSendClearBtn, &QPushButton::clicked, this, [=]() { ui->comSendText->clear(); });

    // 发送区发送数据按钮
    connect(ui->comSendBtn, &QPushButton::clicked, this, &Widget::comSendData);

    /*****************多条发送*************************/
    // currentIndexChanged信号有二义性可以使用QOverload<type>::of转换函数指针指明使用的信号
    connect(ui->comSendMode, &QComboBox::currentIndexChanged, this,
            [=](int index) { ui->comSend->setCurrentIndex(index); });

    // 多项发送当中每一条发送按钮
    for (int i = 1; i < 21; ++i) {
        auto *pPushButton = findChild<QPushButton *>(QString("comSendBtn%1").arg(i));
        if (pPushButton) {
            connect(pPushButton, &QPushButton::clicked, this, &Widget::comSendData);
        }
    }

    // 多项发送全部清除按钮
    connect(ui->comSendClearAllBtn, &QPushButton::clicked, this, [=]() {
        // 多项发送
        for (int i = 1; i < 21; ++i) {
            auto *pLineEdit = findChild<QLineEdit *>(QString("comSendText%1").arg(i));
            if (pLineEdit) {
                pLineEdit->clear();
            }
        }
    });

    // 多项发送上一页按钮
    connect(ui->comSendPageUpBtn, &QPushButton::clicked, this,
            [=]() { ui->uartMultipleSend->setCurrentIndex(0); });

    // 多项发送上下一页按钮
    connect(ui->comSendPageDownBtn, &QPushButton::clicked, this,
            [=]() { ui->uartMultipleSend->setCurrentIndex(1); });

    // 定时自动发送周期编辑栏
    comAutoSendTimer = new QTimer(this);
    comAutoSendTimer->setObjectName("timer");

    // 正则表达式限制输入框内容
    QRegularExpression regExp(R"(^([1-9][0-9]{1,5})?$)"); // 构造一个正则匹配器(注意原始字面量)
    ui->comSendCycleEdit->setValidator(new QRegularExpressionValidator(
        regExp, ui->comSendCycleEdit)); // 给编辑栏添加正则匹配验证器

    comSendCycle = ui->comSendCycleEdit->text();

    // 定时自动发送周期编辑栏
    connect(ui->comSendCycleEdit, &QLineEdit::editingFinished, this, [=]() {
        if (ui->comSendCycleEdit->text().isNull()) {
            ui->comSendCycleEdit->setText(comSendCycle);
        }
        comSendCycle = ui->comSendCycleEdit->text();
        if (ui->comSendAutoBtn->isChecked() && isComConnected) {
            qint32 time = ui->comSendCycleEdit->text().toInt();
            if (time != 0) {
                comAutoSendTimer->start(time);
            }
        }
    });

    // 定时发送按钮
    connect(ui->comSendAutoBtn, &QCheckBox::stateChanged, this, [=]() {
        if (ui->comSendAutoBtn->isChecked()) {
            qint32 time = ui->comSendCycleEdit->text().toInt();

            // 检查周期是否为0
            if (time != 0 && isComConnected) {
                comAutoSendTimer->start(time);
            }
        } else {
            comAutoSendTimer->stop();
        }
    });

    // 串口定时自动发送
    connect(comAutoSendTimer, &QTimer::timeout, this, [=]() {
        if (isComConnected) {
            comSendData();
        }
    });
}

/**
 * @brief 通信界面发送按钮有效控制
 * @param state 是否有效
 */
void Widget::setComUiIsEnabled(bool state)
{
    /**********发送按钮******************/
    ui->comSendBtn->setEnabled(state);
    ui->comSendBtn1->setEnabled(state);
    ui->comSendBtn2->setEnabled(state);
    ui->comSendBtn3->setEnabled(state);
    ui->comSendBtn4->setEnabled(state);
    ui->comSendBtn5->setEnabled(state);
    ui->comSendBtn6->setEnabled(state);
    ui->comSendBtn7->setEnabled(state);
    ui->comSendBtn8->setEnabled(state);
    ui->comSendBtn9->setEnabled(state);
    ui->comSendBtn10->setEnabled(state);
    ui->comSendBtn11->setEnabled(state);
    ui->comSendBtn12->setEnabled(state);
    ui->comSendBtn13->setEnabled(state);
    ui->comSendBtn14->setEnabled(state);
    ui->comSendBtn15->setEnabled(state);
    ui->comSendBtn16->setEnabled(state);
    ui->comSendBtn17->setEnabled(state);
    ui->comSendBtn18->setEnabled(state);
    ui->comSendBtn19->setEnabled(state);
    ui->comSendBtn20->setEnabled(state);
}

/**
 * @brief 初始化串口助手界面
 */
void Widget::uartInit()
{
    /*************调整串口默认参数*******************************/
    ui->uartBaudRate->setCurrentIndex(6);
    ui->uartDataBit->setCurrentIndex(3);
    setUartUiIsEnabled(true);

    /*************调整串口下拉控件样式*******************************/
    ui->uartPortName->setView(new QListView());
    ui->uartBaudRate->setView(new QListView());
    ui->uartCheckBit->setView(new QListView());
    ui->uartDataBit->setView(new QListView());
    ui->uartStopBit->setView(new QListView());

    /**************填充可用串口*****************************/
    auto ports = SerialPortInfo::availablePorts();
    if (!ports.empty()) {
        ui->uartPortName->addItems(SerialPortInfo::availablePorts());
        /**************添加串口ToolTip*****************************/
        auto *model = new QStandardItemModel();
        for (int i = 0; i < ui->uartPortName->count(); i++) {
            auto *item = new QStandardItem(ui->uartPortName->itemText(i));
            item->setToolTip(ui->uartPortName->itemText(i)); // 设置提示文本
            model->appendRow(item);                          // 将item添加到model中
        }
        ui->uartPortName->setModel(model); // 下拉框设置model
    }

    /**************连接串口信号*****************************/
    // 刷新可用串口
    connect(serialPortInfo, &SerialPortInfo::update, this, [=](const QStringList &ports) {
        // 先移除再添加
        ui->uartPortName->clear();
        if (!ports.empty()) {
            ui->uartPortName->addItems(SerialPortInfo::availablePorts());
            auto *model = new QStandardItemModel();
            for (int i = 0; i < ui->uartPortName->count(); i++) {
                auto *item = new QStandardItem(ui->uartPortName->itemText(i));
                item->setToolTip(ui->uartPortName->itemText(i)); // 设置提示文本
                model->appendRow(item);                          // 将item添加到model中
            }
            ui->uartPortName->setModel(model); // 下拉框设置model
        }
    });

    // 串口自动断开
    connect(serialPortInfo, &SerialPortInfo::disconnected, this, [=](const QString &port) {
        if (isComConnected && port == ui->uartPortName->currentText()) // 串口断开
        {
            comAutoSendTimer->stop();
            isComConnected = false;
            emit closeComThread();
            comThread->quit();
            comThread->wait();
            QMessageBox::warning(this, tr("警告"), tr("串口断开连接"));
            setUartUiIsEnabled(true);                    // 改变控件的状态
            ui->uartOperateBtn->setText(tr("打开串口")); // 改变按钮文字
        }
    });

    /************串口界面控按钮信号与槽连接*****************************/
    // 打开串口按钮
    connect(ui->uartOperateBtn, &QPushButton::clicked, this, [=]() {
        if (!isComConnected) // 打开串口
        {
            if (ui->uartPortName->count() != 0) {
                comConfig.clear();
                comConfig.append(QString::number(COM_TYPE::UART));
                comConfig.append(ui->uartPortName->currentText());
                comConfig.append(ui->uartBaudRate->currentText());
                comConfig.append(QString::number(ui->uartCheckBit->currentIndex()));
                comConfig.append(ui->uartDataBit->currentText());
                comConfig.append(QString::number(ui->uartStopBit->currentIndex()));
                serialPortInfo->registerUsingSerialPort(ui->uartPortName->currentText());
                emit startComThread(comConfig);
            } else {
                QMessageBox::critical(this, tr("错误"), tr("当前无串口设备"));
            }
        } else // 关闭串口
        {
            isComConnected = false;
            emit closeComThread();
            comThread->quit();
            comThread->wait();
            setUartUiIsEnabled(true);
            serialPortInfo->unregisterUsingSerialPort(ui->uartPortName->currentText());
            ui->uartOperateBtn->setText(tr("打开串口"));
        }
    });
}

/**
 * @brief 网络通信初始化
 */
void Widget::netInit()
{
    // 使下拉选框可以编辑
    ui->netServerLocalIp->setEditable(true);
    ui->netUdpLocalIp->setEditable(true);

    // 更新可用IP
    ui->netServerLocalIp->addItems(Network::getAllLocalIp());
    ui->netUdpLocalIp->addItems(Network::getAllLocalIp());

    // 调整下拉控件样式
    ui->netServerLocalIp->setView(new QListView());
    ui->netUdpLocalIp->setView(new QListView());

    /*************网络通信界面UI控件信号与槽连接**********************/
    // 启动TCP服务器按钮
    connect(ui->netServerOpBtn, &QPushButton::clicked, this, [=]() {
        if (ui->netServerLocalIp->isEnabled()) {
            comConfig.clear();
            comConfig.append(QString::number(COM_TYPE::TCP_SERVER)); // 通信类型
            comConfig.append(ui->netServerLocalIp->currentText());   // 本地IP
            comConfig.append(ui->netServerLocalPort->text());        // 本地端口

            /************通信配置正确性验证******************/
            if (QHostAddress(comConfig[1]).isNull()) {
                QMessageBox::critical(this, tr("错误"), tr("非法本地IP"));
                return;
            }
            if (comConfig[2].toUInt() == 0) {
                QMessageBox::critical(this, tr("错误"), tr("非法本地端口"));
                return;
            }

            emit startComThread(comConfig);
            ui->netServerOpBtn->setText(tr("停止监听"));
            ui->netServerLocalIp->setEnabled(false);
            ui->netServerLocalPort->setEnabled(false);
        } else // 停止监听或断开连接
        {
            isComConnected = false;
            emit closeComThread();
            comThread->quit();
            comThread->wait();

            ui->netServerOpBtn->setText(tr("开始监听"));
            ui->netServerLocalIp->setEnabled(true);
            ui->netServerLocalPort->setEnabled(true);
            ui->netServerStateBtn->setChecked(false);
            setComUiIsEnabled(false);
        }
    });

    // 启动关闭TCP客户端按钮
    connect(ui->netClientOpBtn, &QPushButton::clicked, [=]() {
        if (ui->netClientRemoteIp->isEnabled()) {
            comConfig.clear();
            comConfig.append(QString::number(COM_TYPE::TCP_CLIENT)); // 通信类型
            comConfig.append(ui->netClientRemoteIp->text());         // 远程IP
            comConfig.append(ui->netClientRemotePort->text());       // 远程端口

            /************通信配置正确性验证******************/
            if (QHostAddress(comConfig[1]).isNull()) {
                QMessageBox::critical(this, tr("错误"), tr("非法远程IP"));
                return;
            }
            if (comConfig[2].toUInt() == 0) {
                QMessageBox::critical(this, tr("错误"), tr("非法远程端口"));
                return;
            }

            emit startComThread(comConfig);
        } else {
            isComConnected = false;
            emit closeComThread();
            comThread->quit();
            comThread->wait();

            ui->netClientOpBtn->setText(tr("开始连接"));
            ui->netClientRemoteIp->setEnabled(true);
            ui->netClientRemotePort->setEnabled(true);
            ui->netClientStateBtn->setChecked(false);
            setComUiIsEnabled(false);
        }
    });

    // UDP连接按钮
    connect(ui->netUdpOpBtn, &QPushButton::clicked, [=]() {
        if (ui->netUdpLocalIp->isEnabled()) {
            comConfig.clear();
            comConfig.append(QString::number(COM_TYPE::UDP)); // 通信类型

            comConfig.append(ui->netUdpLocalIp->currentText()); // 本地IP
            comConfig.append(ui->netUdpLocalPort->text());      // 本地端口
            comConfig.append(ui->netUdpRemoteIp->text());       // 远程IP
            comConfig.append(ui->netUdpRemotePort->text());     // 远程端口

            /************通信配置正确性验证******************/
            if (QHostAddress(comConfig[1]).isNull() || QHostAddress(comConfig[3]).isNull()) {
                QMessageBox::critical(this, tr("错误"), tr("非法IP"));
                return;
            }
            if (comConfig[2].toUInt() == 0 && comConfig[4].toUInt() == 0) {
                QMessageBox::critical(this, tr("错误"), tr("非法端口"));
                return;
            }

            emit startComThread(comConfig);
            ui->netUdpOpBtn->setText(tr("关闭"));
            ui->netUdpLocalIp->setEnabled(false);
            ui->netUdpLocalPort->setEnabled(false);
            ui->netUdpRemoteIp->setEnabled(false);
            ui->netUdpRemotePort->setEnabled(false);
        } else {
            ui->netUdpOpBtn->setText(tr("打开"));
            ui->netUdpLocalIp->setEnabled(true);
            ui->netUdpLocalPort->setEnabled(true);
            ui->netUdpRemoteIp->setEnabled(true);
            ui->netUdpRemotePort->setEnabled(true);
            setComUiIsEnabled(false);
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

    setComUiIsEnabled(!state);
}

/**
 * @brief 处理串口线程的信号
 * @param msg 收到的信号
 */
void Widget::comMsgHandle(const COMMON_MSG::MSG &msg)
{
    if (msg == COMMON_MSG::MSG::ReadyRead) // 接收到数据
    {
        if (ui->comRecStopBtn->checkState() != Qt::Checked) // 暂停接收按钮没有被按下
        {
            ui->comRecText->moveCursor(QTextCursor::End);        // 移动光标到末尾
            QByteArray recText = comThread->recBuffer.dequeue(); // 获取接收缓冲区的数据
            recBytes += recText.length(); // 追加接收到的字符字节数量
            ui->recLabel->setText(tr("接收字节数:%1").arg(recBytes));

            /*************16进制显示？********************/
            if (ui->comRecHexBtn->isChecked()) {
                QByteArray_to_HexQByteArray(recText);
            }

            /*************日志模式显示？********************/
            if (ui->comRecLogCheck->isChecked()) {
                ui->comRecText->insertPlainText(QTime::currentTime()
                                                    .toString("hh:mm:ss")
                                                    .prepend('[')
                                                    .append("] ")); // 文末追加文本
                recText.append("\n");
            }
            ui->comRecText->insertPlainText(recText); // 文末追加文本
        } else {
            comThread->recBuffer.clear();
        }
    } else if (msg == COMMON_MSG::MSG::OpenFail) // 打开失败
    {
        QMessageBox::critical(this, tr("错误"), tr("无法打开串口"));
        isComConnected = false;
    } else if (msg == COMMON_MSG::MSG::OpenSuccessful) // 打开成功
    {
        setUartUiIsEnabled(false);                   // 切换控件的状态
        ui->uartOperateBtn->setText(tr("关闭串口")); // 改变按钮文字
        if (ui->comSendAutoBtn->isChecked())         // 定时发送按钮被选中
        {
            comAutoSendTimer->start(ui->comSendCycleEdit->text().toInt());
        }
        isComConnected = true;
        comThread->start();
    } else if (msg == COMMON_MSG::MSG::Connected) // 连接成功
    {
        setComUiIsEnabled(true);
        if (comThread->protocol == COM_TYPE::TCP_SERVER)
            ui->netServerStateBtn->setChecked(true);
        else {
            ui->netClientStateBtn->setChecked(true);
            ui->netClientOpBtn->setText(tr("断开连接"));
            ui->netClientRemoteIp->setEnabled(false);
            ui->netClientRemotePort->setEnabled(false);
        }
        if (ui->comSendAutoBtn->isChecked()) // 定时发送按钮被选中
        {
            comAutoSendTimer->start(ui->comSendCycleEdit->text().toInt());
        }
        isComConnected = true;
        comThread->start();
    } else if (msg == COMMON_MSG::MSG::Disconnected) // 断开连接
    {
        comAutoSendTimer->stop();
        isComConnected = false;
        setComUiIsEnabled(false);
        comThread->quit();
        comThread->wait();
        if (ui->interfaceBtn->currentIndex() == COM_TYPE::TCP_SERVER) {
            ui->netServerStateBtn->setChecked(false);
        } else if (ui->interfaceBtn->currentIndex() == COM_TYPE::TCP_CLIENT) {
            ui->netClientStateBtn->setChecked(false);
            ui->netClientOpBtn->setText(tr("开始连接"));
            ui->netClientRemoteIp->setEnabled(true);
            ui->netClientRemotePort->setEnabled(true);
        }
    }
}

/**
 * @brief 串口发送数据
 */
void Widget::comSendData()
{
    QByteArray sendText;
    QString objName = sender()->objectName(); // 获取发送信号对象的名字

    if (isComConnected) {
        /*************单项发送？********************/
        if (objName == ui->comSendBtn->objectName() || objName == comAutoSendTimer->objectName()) {
            sendText = ui->comSendText->toPlainText().toUtf8();
        } else {
            auto *pLineEdit =
                findChild<QLineEdit *>(objName.replace(QString("Btn"), QString("Text")));
            if (pLineEdit) {
                sendText = pLineEdit->text().toUtf8();
            }
        }

        if (!sendText.isEmpty()) {
            /*************选择追加的字符********************/
            switch (ui->comSendAppendText->currentIndex()) {
            case 1:
                sendText += '\r';
                break;
            case 2:
                sendText += '\n';
                break;
            case 3:
                sendText += "\r\n";
                break;
            case 4:
                sendText += "\n\r";
                break;
            default:
                break;
            }
            if (ui->comSendHexBtn->isChecked()) // 16进制发送
                sendText = sendText.toHex();
        } else {
            return;
        }
        emit sendComDateSignal(sendText);
        sendBytes += sendText.length(); // 累加发送字符字节数
        ui->sendLabel->setText(QString("发送字节数:%1").arg(sendBytes));
    }
}

/**
 * @brief 初始化PID界面
 */
void Widget::pidInit()
{
    /**********设置默认的串口配置********************************/
    ui->pidBaudRate->setCurrentIndex(6);
    ui->pidDataBit->setCurrentIndex(3);

    /*************调整下拉控件样式*******************************/
    ui->pidPortName->setView(new QListView(ui->pidPortName));
    ui->pidBaudRate->setView(new QListView(ui->pidBaudRate));
    ui->pidCheckBit->setView(new QListView(ui->pidCheckBit));
    ui->pidDataBit->setView(new QListView(ui->pidDataBit));
    ui->pidStopBit->setView(new QListView(ui->pidStopBit));
    ui->pidChannelBtn->setView(new QListView(ui->pidChannelBtn));

    /**************填充可用串口*****************************/
    auto ports = SerialPortInfo::availablePorts();
    ui->pidPortName->clear();
    ui->pidPortName->addItems(ports);

    /**************添加串口ToolTip*****************************/
    auto *model = new QStandardItemModel();
    for (int i = 0; i < ui->pidPortName->count(); i++) {
        auto *item = new QStandardItem(ui->pidPortName->itemText(i));
        item->setToolTip(ui->pidPortName->itemText(i)); // 设置提示文本
        model->appendRow(item);                         // 将item添加到model中
    }
    ui->pidPortName->setModel(model); // 下拉框设置model

    /**************连接串口信号*****************************/
    // 刷新可用串口
    connect(serialPortInfo, &SerialPortInfo::update, this, [=](const QStringList &ports) {
        // 先移除再添加
        ui->pidPortName->clear();
        ui->pidPortName->addItems(ports);

        auto *model = new QStandardItemModel();
        for (int i = 0; i < ui->pidPortName->count(); i++) {
            auto *item = new QStandardItem(ui->pidPortName->itemText(i));
            item->setToolTip(ui->pidPortName->itemText(i)); // 设置提示文本
            model->appendRow(item);                         // 将item添加到model中
        }
        ui->pidPortName->setModel(model); // 下拉框设置model
    });
    // 串口断开
    connect(serialPortInfo, &SerialPortInfo::disconnected, this, [=](const QString &port) {
        if (isPidConnected && port == ui->pidPortName->currentText()) // 串口断开
        {
            comAutoSendTimer->stop();
            isPidConnected = false;
            emit closePidThread();
            comThread->quit();
            comThread->wait();
            QMessageBox::warning(this, tr("警告"), tr("串口断开连接"));
            setPidUiIsEnabled(true);                    // 改变控件的状态
            ui->pidOperateBtn->setText(tr("打开串口")); // 改变按钮文字
        }
    });

    /*************用正则表达式限制输入框内容***********************************************/
    QRegularExpression regExp(
        R"(^((\-?)(\d{1,6})(\.?)(\d{1,6}))?$)"); // 正则表达式(注意使用原始字面量)
    ui->pidPEdit->setValidator(new QRegularExpressionValidator(regExp, ui->pidPEdit));
    ui->pidIEdit->setValidator(new QRegularExpressionValidator(regExp, ui->pidIEdit));
    ui->pidDEdit->setValidator(new QRegularExpressionValidator(regExp, ui->pidDEdit));
    regExp.setPattern(R"(^([1-9][0-9]{1,5})?$)");
    ui->pidTargetEdit->setValidator(new QRegularExpressionValidator(regExp, ui->pidTargetEdit));
    ui->pidCycleEdit->setValidator(new QRegularExpressionValidator(regExp, ui->pidCycleEdit));

    this->setPidUiIsEnabled(true);

    /******************保证输入数据非空**************************************/
    connect(ui->pidPEdit, &QLineEdit::editingFinished, this, [=]() {
        if (ui->pidPEdit->text().isNull()) {
            ui->pidPEdit->setText(pidChannel[ui->pidChannelBtn->currentIndex()]->p);
        }
        pidChannel[ui->pidChannelBtn->currentIndex()]->p = ui->pidPEdit->text();
    });
    connect(ui->pidIEdit, &QLineEdit::editingFinished, this, [=]() {
        if (ui->pidIEdit->text().isNull()) {
            ui->pidIEdit->setText(pidChannel[ui->pidChannelBtn->currentIndex()]->i);
        }
        pidChannel[ui->pidChannelBtn->currentIndex()]->i = ui->pidIEdit->text();
    });
    connect(ui->pidDEdit, &QLineEdit::editingFinished, this, [=]() {
        if (ui->pidDEdit->text().isNull()) {
            ui->pidDEdit->setText(pidChannel[ui->pidChannelBtn->currentIndex()]->d);
        }
        pidChannel[ui->pidChannelBtn->currentIndex()]->d = ui->pidDEdit->text();
    });
    connect(ui->pidTargetEdit, &QLineEdit::editingFinished, this, [=]() {
        if (ui->pidTargetEdit->text().isNull()) {
            ui->pidTargetEdit->setText(pidChannel[ui->pidChannelBtn->currentIndex()]->target);
        }
        pidChannel[ui->pidChannelBtn->currentIndex()]->target = ui->pidTargetEdit->text();
    });
    connect(ui->pidCycleEdit, &QLineEdit::editingFinished, this, [=]() {
        if (ui->pidCycleEdit->text().isNull()) {
            ui->pidCycleEdit->setText(pidChannel[ui->pidChannelBtn->currentIndex()]->cycle);
        }
        pidChannel[ui->pidChannelBtn->currentIndex()]->cycle = ui->pidCycleEdit->text();
        pidCycleKeyStep = pidChannel[ui->pidChannelBtn->currentIndex()]->cycle.toDouble() / 1000.0;
    });

    /****************按钮槽连接**************************************************/
    // 打开串口按钮
    connect(ui->pidOperateBtn, &QPushButton::clicked, this, [=]() {
        if (!isPidConnected) // 打开串口
        {
            if (ui->pidPortName->count() != 0) {
                pidConfig.clear();
                pidConfig.append(QString("uart"));
                pidConfig.append(ui->pidPortName->currentText());
                pidConfig.append(ui->pidBaudRate->currentText());
                pidConfig.append(QString::number(ui->pidCheckBit->currentIndex()));
                pidConfig.append(ui->uartDataBit->currentText());
                pidConfig.append(QString::number(ui->pidStopBit->currentIndex()));
                serialPortInfo->registerUsingSerialPort(ui->pidPortName->currentText());
                emit startPidThread(pidConfig);
            } else {
                QMessageBox::warning(this, tr("错误"), tr("当前无串口设备"));
            }
        } else // 关闭串口
        {
            isPidConnected = false;
            emit closePidThread();
            comThread->quit();
            comThread->wait();
            ui->pidStartBtn->click();
            setPidUiIsEnabled(true);
            serialPortInfo->unregisterUsingSerialPort(ui->pidPortName->currentText());
            ui->pidOperateBtn->setText(tr("打开串口"));
        }
    });
    connect(ui->pidSendTargetBtn, &QPushButton::clicked, this,
            [=]() // 发送目标按钮
    { this->pidSendData(PID_MASTER::Target); });
    connect(ui->pidResetBtn, &QPushButton::clicked, this,
            [=]() // 复位按钮
    { this->pidSendData(PID_MASTER::Reset); });
    connect(ui->pidSendParamBtn, &QPushButton::clicked, this,
            [=]() // 发送PID按钮
    { this->pidSendData(PID_MASTER::Param); });
    connect(ui->pidStartBtn, &QPushButton::clicked, this,
            [=]() // 启动与停止按钮
    {
        qDebug() << isPidStart;
        if (isPidConnected) {
            if (isPidStart) {
                this->pidSendData(PID_MASTER::Start);
                ui->pidStartBtn->setText(tr("停止"));
            } else {
                this->pidSendData(PID_MASTER::Stop);
                ui->pidStartBtn->setText(tr("启动"));
            }
            isPidStart = !isPidStart;
        } else if (!isPidStart) {
            ui->pidStartBtn->setText(tr("启动"));
            isPidStart = !isPidStart;
        }
    });
    connect(ui->pidSendCycleBtn, &QPushButton::clicked, this,
            [=]() // 发送周期按钮
    { this->pidSendData(PID_MASTER::Cycle); });

    /****************曲线处理********************************/
    ui->customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom |
                                    QCP::iMultiSelect); // 可拖拽+可滚轮缩放+可以被选中

    ui->customPlot->setBackground(QColor(48, 48, 48)); // 设置背景颜色

    /**************坐标轴样式设定**************************/
    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%h:%m:%s"); // 显示格式 时：分：秒
    ui->customPlot->xAxis->setTicker(timeTicker);
    ui->customPlot->xAxis->setRange(0.0, 5.0);            // 最小单位是秒
    ui->customPlot->xAxis->setTickPen(QPen(Qt::white));   // 设置刻度画笔
    ui->customPlot->xAxis->setTickLabelColor(Qt::yellow); // 设置坐标轴文字颜色
    ui->customPlot->xAxis->setBasePen(QPen(Qt::green));   // 设置坐标轴基准线画笔

    ui->customPlot->yAxis->setTickLabelColor(Qt::yellow); // 设置坐标轴数值颜色
    ui->customPlot->yAxis->setBasePen(QPen(Qt::green));   // 设置坐标轴基准线画笔
    ui->customPlot->yAxis->setRange(0.0, 100);
    ui->customPlot->yAxis->ticker()->setTickCount(15);               // 设置刻度数量
    ui->customPlot->yAxis->grid()->setZeroLinePen(QPen(Qt::yellow)); // 设置横坐标零刻度画笔

    /****************添加四条曲线*****************************/
    ui->customPlot->addGraph();                        // 添加一条曲线
    ui->customPlot->graph(0)->setPen(QPen(Qt::green)); // 曲线蓝色
    ui->customPlot->graph(0)->setName("CH1");          // 图例名称
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
    ui->customPlot->legend->setFillOrder(QCPLayoutGrid::foColumnsFirst); // 设置图例优先按排放置
    ui->customPlot->legend->setWrap(
        4); // 优先规则中最大数量(一排最多数量或则一列最多数量，取决于优先放置规则)
    ui->customPlot->plotLayout()->addElement(
        1, 0, ui->customPlot->legend); // 图例和坐标图按栅格布局，前两个参数控制元素在栅格的位置
    ui->customPlot->plotLayout()->setRowStretchFactor(1, 0.01); // 设置显示比例
    ui->customPlot->legend->setBorderPen(Qt::NoPen);            // 设置边框隐藏
    ui->customPlot->legend->setVisible(true);                   // 图例可见
    ui->customPlot->legend->setBrush(QColor(255, 255, 255, 0));
    ui->customPlot->legend->setTextColor(QColor(220, 220, 220));

    /*************面板控件设置曲线*****************************/
    connect(ui->pidClearDisplayBtn, &QPushButton::clicked, this,
            [=]() // 清除所有曲线数据点并还原坐标
    {
        ui->customPlot->graph(0)->data().data()->clear(); // 获取数据容器并清除数据
        ui->customPlot->graph(1)->data().data()->clear();
        ui->customPlot->graph(2)->data().data()->clear();
        ui->customPlot->graph(3)->data().data()->clear();
        ui->customPlot->xAxis->setRange(0, 5); // 还原横坐标
        ui->customPlot->replot();
        pidCycleKey = 0.0;
    });
    connect(ui->pidActualCH1, &QCheckBox::clicked, this,
            [=]() // 通道1实际值显示
    {
        if (ui->pidActualCH1->checkState() == Qt::Checked) {
            ui->customPlot->graph(0)->setVisible(true);
        } else {
            ui->customPlot->graph(0)->setVisible(false);
        }
        ui->customPlot->replot();
    });
    connect(ui->pidActualCH2, &QCheckBox::clicked, this,
            [=]() // 通道2实际值显示
    {
        if (ui->pidActualCH2->checkState() == Qt::Checked) {
            ui->customPlot->graph(1)->setVisible(true);
        } else {
            ui->customPlot->graph(1)->setVisible(false);
        }
        ui->customPlot->replot();
    });
    connect(ui->pidActualCH3, &QCheckBox::clicked, this,
            [=]() // 通道3实际值显示
    {
        if (ui->pidActualCH3->checkState() == Qt::Checked) {
            ui->customPlot->graph(2)->setVisible(true);
        } else {
            ui->customPlot->graph(2)->setVisible(false);
        }
        ui->customPlot->replot();
    });
    connect(ui->pidActualCH4, &QCheckBox::clicked, this,
            [=]() // 通道4实际值显示
    {
        if (ui->pidActualCH4->checkState() == Qt::Checked) {
            ui->customPlot->graph(3)->setVisible(true);
        } else {
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
    connect(ui->pidTargetCH1, &QCheckBox::clicked, this, [=]() {
        if (ui->pidTargetCH1->checkState() == Qt::Checked) {
            targetLineCH1->start->setCoords(0, pidChannel[0]->target.toDouble());
            targetLineCH1->end->setCoords(10000, pidChannel[0]->target.toDouble());
            targetLineCH1->setVisible(true);
        } else {
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
    connect(ui->pidTargetCH2, &QCheckBox::clicked, this, [=]() {
        if (ui->pidTargetCH2->checkState() == Qt::Checked) {
            targetLineCH2->start->setCoords(0, pidChannel[1]->target.toDouble());
            targetLineCH2->end->setCoords(10000, pidChannel[1]->target.toDouble());
            targetLineCH2->setVisible(true);
        } else {
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
    connect(ui->pidTargetCH3, &QCheckBox::clicked, this, [=]() {
        if (ui->pidTargetCH3->checkState() == Qt::Checked) {
            targetLineCH3->start->setCoords(0, pidChannel[2]->target.toDouble());
            targetLineCH3->end->setCoords(10000, pidChannel[2]->target.toDouble());
            targetLineCH3->setVisible(true);
        } else {
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
    connect(ui->pidTargetCH4, &QCheckBox::clicked, this, [=]() {
        if (ui->pidTargetCH4->checkState() == Qt::Checked) {
            targetLineCH4->start->setCoords(0, pidChannel[3]->target.toDouble());
            targetLineCH4->end->setCoords(10000, pidChannel[3]->target.toDouble());
            targetLineCH4->setVisible(true);
        } else {
            targetLineCH4->setVisible(false);
        }
        ui->customPlot->replot();
    });

    ui->customPlot->replot(); // 刷新曲线
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

    // 发送区与串口配置区相反
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
void Widget::pidMsgHandle(const COMMON_MSG::MSG &msg)
{
    if (msg == COMMON_MSG::MSG::ReadyRead) // 接收到数据
    {
        QStringList strList = pidThread->recBuffer.dequeue(); // 获取接收缓冲区的数据;
        switch (strList[0].toUShort()) {
        case PID_SLAVE::Target: // 同步目标值
            ui->pidTargetEdit->setText(strList[2]);
            break;
        case PID_SLAVE::Actual:                                     // 同步实际值
            if (ui->pidStopDisplayBtn->checkState() != Qt::Checked) // 停止显示按钮没有被按下
            {
                pidCycleKey += pidCycleKeyStep;
                ui->customPlot->graph(strList[1].toInt() - 1)
                    ->addData(pidCycleKey,
                              strList[2].toDouble()); // 添加数据到对应曲线

                // 自动设定y轴的范围，如果不设定，有可能看不到图像
                // 也可以用ui->customPlot->yAxis->setRange(up,low)手动设定y轴范围
                ui->customPlot->graph(0)->rescaleValueAxis();
                ui->customPlot->graph(1)->rescaleValueAxis(true);
                ui->customPlot->graph(2)->rescaleValueAxis(true);
                ui->customPlot->graph(3)->rescaleValueAxis(true);

                // 自动设定y轴的范围，如果不设定，有可能看不到图像
                // 也可以用ui->customPlot->yAxis->setRange(up,low)手动设定y轴范围
                if (pidCycleKey > 3)
                    ui->customPlot->xAxis->setRange(pidCycleKey + 2, 5,
                                                    Qt::AlignRight); // 设定x轴的范围实现自动滚动
                ui->customPlot->replot(QCustomPlot::rpQueuedReplot); // 刷新图行
            }
            break;
        case PID_SLAVE::Param: // 同步PID参数
            ui->pidPEdit->setText(strList[2]);
            ui->pidIEdit->setText(strList[3]);
            ui->pidDEdit->setText(strList[4]);
            break;
        case PID_SLAVE::Start: // 同步启动按钮
            isPidStart = true;
            ui->pidStartBtn->click();
            break;
        case PID_SLAVE::Stop: // 同步停止按钮
            isPidStart = false;
            ui->pidStartBtn->click();
            break;
        case PID_SLAVE::Cycle: // 同步PID执行周期
            if (strList[2].toDouble() > 0.0) {
                ui->pidCycleEdit->setText(strList[2]);
                pidCycleKeyStep = strList[2].toDouble() / 1000.0;
            }
            break;
        default:
            break;
        }
    } else if (msg == COMMON_MSG::MSG::OpenFail) // 打开失败
    {
        QMessageBox::warning(this, tr("警告"), tr("无法打开串口"));
        isPidConnected = false;
    } else if (msg == COMMON_MSG::MSG::OpenSuccessful) // 打开成功
    {
        setPidUiIsEnabled(false);                   // 改变控件的状态
        ui->pidOperateBtn->setText(tr("关闭串口")); // 改变按钮文字
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
    sendText += "(" + QByteArray::number(ui->pidChannelBtn->currentIndex() + 1) + "|";
    switch (cmd) {
    case PID_MASTER::Target:
        sendText += QByteArray::number(PID_MASTER::Target) + "|";
        sendText += ui->pidTargetEdit->text().toUtf8() + "|";
        break;
    case PID_MASTER::Reset:
        sendText += QByteArray::number((quint8)PID_MASTER::Reset) + "|";
        break;
    case PID_MASTER::Param:
        sendText += QByteArray::number(PID_MASTER::Param) + "|";
        sendText += QByteArray::number(ui->pidPEdit->text().toFloat()) + "|";
        sendText += QByteArray::number(ui->pidIEdit->text().toFloat()) + "|";
        sendText += QByteArray::number(ui->pidDEdit->text().toFloat()) + "|";

#ifndef QT_NO_DEBUG_OUTPUT
        qDebug()
            << "pidPEdit"
            << atof(QString::asprintf("%f", ui->pidPEdit->text().toFloat()).toStdString().data());
        qDebug()
            << "pidIEdit"
            << atof(QString::asprintf("%f", ui->pidIEdit->text().toFloat()).toStdString().data());
        qDebug()
            << "pidDEdit"
            << atof(QString::asprintf("%f", ui->pidDEdit->text().toFloat()).toStdString().data());
#endif
        break;
    case PID_MASTER::Start:
        sendText += QByteArray::number(PID_MASTER::Start) + "|";
        break;
    case PID_MASTER::Stop:
        sendText += QByteArray::number(PID_MASTER::Stop) + "|";
        break;
    case PID_MASTER::Cycle:
        sendText += QByteArray::number(PID_MASTER::Cycle) + "|";
        sendText += QByteArray::number(ui->pidCycleEdit->text().toUInt()) + "|";
        break;
    default:
        break;
    }
    if (sendText.size() + 2 >= 10) {
        sendText += QByteArray::number(sendText.size() + 3) + ")";
    } else {
        sendText += QByteArray::number(sendText.size() + 2) + ")";
    }
    qDebug() << "sendText_size=" << sendText.size();
    emit sendPidDateSignal(sendText);
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

    ui->oscIp->setEditable(true);              // 使下拉选框可以编辑
    ui->oscIp->addItems(OSC::getAllLocalIp()); // 更新可用IP

    /*************网络通信界面UI控件信号与槽连接**********************/
    connect(ui->oscProtocol, &QComboBox::currentTextChanged, this, [=]() {
        static quint8 oscIndex = 0;
        qDebug() << oscIndex;
        switch (ui->oscProtocol->currentIndex()) {
        case 0:
            ui->oscOperateBtn->setText(tr("开始监听"));
            if (oscIndex == 2) {
                oscAddUdp(false);
            }
            oscAddTcpServer(true);
            oscIndex = 0;
            break;
        case 1:
            ui->oscOperateBtn->setText(tr("开始连接"));
            if (oscIndex == 0) {
                oscAddTcpServer(false);
            } else {
                oscAddUdp(false);
            }
            oscIndex = 1;
            break;
        case 2:
            ui->oscOperateBtn->setText(tr("打开"));
            if (oscIndex == 0) {
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
    connect(ui->oscOperateBtn, &QPushButton::clicked, this, [=]() {
        if (ui->oscProtocol->isEnabled()) {
            switch (ui->oscProtocol->currentIndex()) {
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
            oscConfig.append(QString::number(ui->oscProtocol->currentIndex())); // 通信类型
            oscConfig.append(ui->oscIp->currentText()); // 本地IP(三种通信)
            oscConfig.append(ui->oscPort->text());      // 本地端口(三种通信)

            /************通信配置正确性验证******************/
            if (ui->oscProtocol->currentIndex() == 2) // UDP通信
            {
                oscConfig.append(oscUdpDesIp->text()); // UDP远程IP
                oscUdpClientIp = QHostAddress(oscConfig[3]);
                if (oscUdpClientIp.isNull()) {
                    QMessageBox::warning(this, tr("错误"), tr("UDP远程IP填写错误"));
                    return;
                }
                oscConfig.append(oscUdpDesPort->text()); // UDP远程端口
                if (ui->oscPort->text().toUInt() == 0 && oscUdpDesPort->text().toUInt() == 0) {
                    QMessageBox::warning(this, tr("错误"), tr("端口填写错误"));
                    return;
                }
            } else if (ui->oscPort->text().toUInt() == 0) // TCP通信端口
            {
                QMessageBox::warning(this, tr("错误"), tr("本地端口填写错误"));
                return;
            }

            emit startOscThread(oscConfig); // NOLINT(readability-misleading-indentation)
            setOscConfigUiIsEnabled(false);
        } else // 停止监听或断开连接
        {
            oscConnectFlag = false;
            emit closeOscThread();
            oscThread->quit();
            oscThread->wait();
            setOscConfigUiIsEnabled(true);
            oscConnectUiIsEnabled(false);
            switch (ui->oscProtocol->currentIndex()) {
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

    /****************曲线处理********************************/
    ui->oscCustomPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom |
                                       QCP::iMultiSelect); // 可拖拽+可滚轮缩放+可以被选中

    ui->oscCustomPlot->setBackground(QColor(48, 48, 48)); // 设置背景颜色

    // 背景网格样式
    ui->oscCustomPlot->xAxis->grid()->setPen(QPen(QColor(50, 50, 50), 1, Qt::SolidLine));
    ui->oscCustomPlot->yAxis->grid()->setPen(QPen(Qt::SolidLine));
    ui->oscCustomPlot->yAxis->grid()->setZeroLinePen(QPen(Qt::yellow, 1)); // y轴0刻度颜色
    ui->oscCustomPlot->xAxis->grid()->setSubGridPen(
        QPen(QColor(50, 50, 50), 1, Qt::DotLine)); // 子网格浅色点线
    ui->oscCustomPlot->yAxis->grid()->setSubGridPen(
        QPen(QColor(50, 50, 50), 1, Qt::DotLine));             // 子网格浅色点线
    ui->oscCustomPlot->xAxis->grid()->setSubGridVisible(true); // 显示x轴子网格线
    ui->oscCustomPlot->yAxis->grid()->setSubGridVisible(true); // 显示y轴子网格线
    ui->oscCustomPlot->xAxis->setTickPen(QPen(Qt::white));     // 设置刻度画笔
    ui->oscCustomPlot->xAxis->setTickLabelColor(Qt::yellow);   // 设置坐标轴文字颜色
    ui->oscCustomPlot->xAxis->setBasePen(QPen(Qt::green));     // 设置坐标轴基准线画笔
    ui->oscCustomPlot->yAxis->setTickLabelColor(Qt::yellow);   // 设置坐标轴数值颜色
    ui->oscCustomPlot->yAxis->setBasePen(QPen(Qt::green));     // 设置坐标轴基准线画笔
    ui->oscCustomPlot->yAxis->setRange(0.0, 100);

    // ui->oscCustomPlot->xAxis->setTickLabels(false);    //横坐标值不可见

    // customplot->xAxis->ticker()->setTickOrigin(1);//改变刻度原点为1

    ui->oscCustomPlot->xAxis->setRange(0, 940, Qt::AlignLeft); // 设定x轴的范围
    ui->oscCustomPlot->yAxis->setRange(-210, 210);             // 设定y轴的范围

    /****************添加两条曲线*****************************/
    ui->oscCustomPlot->addGraph();                      // 添加一条曲线
    ui->oscCustomPlot->graph(0)->setPen(QPen(Qt::red)); // 曲线红色
    ui->oscCustomPlot->graph(0)->setName("CH1");        // 图例名称
    ui->oscCustomPlot->graph(0)->setVisible(true);      // 曲线有效显示
    ui->oscCustomPlot->addGraph();
    ui->oscCustomPlot->graph(1)->setPen(QPen(Qt::blue));
    ui->oscCustomPlot->graph(1)->setName("CH2");
    ui->oscCustomPlot->graph(1)->setVisible(true);

    /*************曲线图例设置****************************/
    ui->oscCustomPlot->legend->setFillOrder(QCPLayoutGrid::foRowsFirst); // 设置图例优先按照列放置
    ui->oscCustomPlot->legend->setWrap(
        4); // 优先规则中最大数量(一排最多数量或一列最多数量，取决于优先放置规则)
    ui->oscCustomPlot->axisRect()->insetLayout()->setInsetAlignment(
        0, Qt::AlignRight | Qt::AlignTop);              // 图例放置在右上角
    ui->oscCustomPlot->legend->setBorderPen(Qt::NoPen); // 设置边框隐藏
    ui->oscCustomPlot->legend->setVisible(true);        // 图例可见
    ui->oscCustomPlot->replot();                        // 刷新曲线
}

/**
 * @brief 是否显示连上TCP服务器的客户端控件
 * @param state 控件显示状态
 *      @arg true 显示状态
 *      @arg false 不显示状态
 */
void Widget::oscAddTcpServer(bool state)
{
    if (state) {
        oscTcpConClient->setParent(ui->oscConfigWidget);
        oscTcpDisBtn->setParent(ui->oscConfigWidget);
        oscTcpClient->setParent(ui->oscConfigWidget);
        oscTcpGridLayout->addWidget(oscTcpConClient, 0, 0, 1, 1);
        oscTcpGridLayout->addWidget(oscTcpDisBtn, 0, 1, 1, 1);
        oscTcpGridLayout->addWidget(oscTcpClient, 1, 0, 1, 2);
        ui->verticalLayout_8->addLayout(oscTcpGridLayout);
    } else {
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
    if (state) {
        oscUdpDesLabel->setParent(ui->oscConfigWidget);
        oscUdpDesIp->setParent(ui->oscConfigWidget);
        oscUdpDesPortLabel->setParent(ui->oscConfigWidget);
        oscUdpDesPort->setParent(ui->oscConfigWidget);
        oscUdpGridLayout->addWidget(oscUdpDesLabel, 0, 0, 1, 1);
        oscUdpGridLayout->addWidget(oscUdpDesIp, 0, 1, 1, 1);
        oscUdpGridLayout->addWidget(oscUdpDesPortLabel, 1, 0, 1, 1);
        oscUdpGridLayout->addWidget(oscUdpDesPort, 1, 1, 1, 1);
        ui->verticalLayout_8->addLayout(oscUdpGridLayout);
    } else {
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
void Widget::oscConnectUiIsEnabled(bool state) { ui->oscStateBtn->setChecked(state); }

/**
 * @brief 处理网络通信消息
 * @param msg 收到的消息
 */
void Widget::oscMsgHandle(const COMMON_MSG::MSG &msg)
{
    if (msg == COMMON_MSG::MSG::ReadyRead) // 可以读到数据
    {
        if (ui->oscRecStopBtn->checkState() != Qt::Checked) // 暂停接收按钮没有被按下
        {
            QByteArray recText = oscThread->recBuffer.dequeue(); // 获取接收缓冲区的数据
            QVector<double> xData, yData;

            for (int i = 0; i < recText.size(); i++) {
                xData.append(i);
                yData.append(static_cast<quint8>(recText[i])); // 只能获取字符型，需要转换
            }

            ui->oscCustomPlot->graph(0)->setData(xData,
                                                 yData); // 添加数据到对应曲线（带清空数据容器）

            // 自动设定y轴的范围，如果不设定，有可能看不到图像
            // 也可以用ui->oscCustomPlot->yAxis->setRange(up,low)手动设定y轴范围
            ui->oscCustomPlot->replot(QCustomPlot::rpQueuedReplot); // 刷新图形
        } else {
            oscThread->recBuffer.clear();
        }
    } else if (msg == COMMON_MSG::MSG::Connected) // 连接成功
    {
        oscConnectFlag = true;
        oscThread->start();
        oscConnectUiIsEnabled(true);
    } else if (msg == COMMON_MSG::MSG::Disconnected) // 断开连接
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

    //    if (ui->netSendNewlineBtn->checkState() == Qt::Checked) // 发送新行被选中
    //        sendText += '\n';
    //
    //    if (ui->netSendHexBtn->isChecked()) // 16进制发送
    //        sendText = sendText.toHex();

    emit sendOscDateSignal(sendText);
    sendBytes += sendText.length(); // 累加发送字符字节数
    ui->sendLabel->setText(QString("发送字节数:%1").arg(sendBytes));
}

/**
 * @brief 下载界面初始化
 */
void Widget::downloadInit()
{
    ui->fwInfo->setColumnWidth(0, 600);
    ui->fwInfo->setColumnWidth(1, 99);
    ui->fwInfo->setColumnWidth(2, 99);

    ui->fwInfo->setItem(0, 0, new QTableWidgetItem());
    ui->fwInfo->setItem(0, 1, new QTableWidgetItem());
    ui->fwInfo->setItem(0, 2, new QTableWidgetItem());
    ui->fwInfo->setItem(0, 3, new QTableWidgetItem());

    ui->fwInfo->item(0, 1)->setToolTip(tr("以0x开始的16进制地址"));
    ui->fwInfo->item(0, 2)->setToolTip(tr("以0x开始的16进制地址"));
    ui->fwInfo->item(0, 3)->setToolTip(tr("以字节为单位"));

    ui->fwInfo->setRowHeight(0, 30);

    // 选择文件按钮
    connect(ui->dowFileBtn, &QToolButton::clicked, this, [=]() {
        QString fileName = QFileDialog::getOpenFileName(this, tr("选择文件"), QDir::home().path(),
                                                        tr("Program file (*.bin *.hex)"));
        if (!fileName.isNull()) {
            FwFileInfo fwFileInfo;
            ui->fwInfo->item(0, 0)->setText(fileName);

            /***************hex文件需要转换****************/
            if (fileName.endsWith(".hex", Qt::CaseInsensitive)) {
                QString binFileName = fileName;
                QString binFileNameTemp = fileName;

                binFileNameTemp = binFileNameTemp.replace(".hex", "temp.bin");

                RESULT_STATUS res = HexFile2BinFile(fileName.toUtf8().data(),
                                                    binFileNameTemp.toUtf8().data(), &fwFileInfo);

                switch (res) {
                case RES_OK:
                    ui->fwInfo->item(0, 1)->setText(
                        QString("0x%1").arg(fwFileInfo.StartAddr, 0, 16));
                    ui->fwInfo->item(0, 2)->setText(QString("0x%1").arg(fwFileInfo.EndAddr, 0, 16));
                    ui->fwInfo->item(0, 3)->setText(QString("%1").arg(fwFileInfo.FileSize));
                    binFileName.replace(".hex",
                                        QString("_0x%1.bin").arg(fwFileInfo.StartAddr, 0, 16),
                                        Qt::CaseInsensitive);
                    if (QFile::exists(binFileName)) {
                        qDebug() << "exists" << binFileName;
                        QFile::remove(binFileName);
                    }
                    QFile::rename(binFileNameTemp, binFileName);
                    break;
                case RES_HEX_FILE_NOT_EXIST:
                case RES_BIN_FILE_PATH_ERROR:
                    QMessageBox::critical(this, tr("错误"), tr("文件不存在"));
                    break;
                default:
                    QMessageBox::critical(this, tr("错误"), tr("文件解析错误"));
                    break;
                }
            }
        }
    });

    // 清除接收日志文本
    connect(ui->dowClearLogBtn, &QPushButton::clicked, this, [=]() {
        ui->dowLogText->clear();
        ui->dowProgressBar->setValue(0);
    });

    // 置零下载进度
    ui->dowProgressBar->setRange(0, 100);
    ui->dowProgressBar->reset();

    /**************ISP界面*****************************/
    ui->ispDowBaudRate->setCurrentIndex(5);
    ui->ispDowHWBtn->setCurrentIndex(3);
    ui->ispDowReadInfoBtn->setEnabled(false);
    ui->ispDowFlashBtn->setEnabled(false);
    ui->ispDowEraseBtn->setEnabled(false);

    // 刷新可用串口
    auto isp_ports = SerialPortInfo::availablePorts();
    if (!isp_ports.empty()) {
        ui->ispDowPortName->addItems(isp_ports);
        auto *model = new QStandardItemModel();
        for (int i = 0; i < ui->ispDowPortName->count(); i++) {
            auto *item = new QStandardItem(ui->ispDowPortName->itemText(i));
            item->setToolTip(ui->ispDowPortName->itemText(i)); // 设置提示文本
            model->appendRow(item);                            // 将item添加到model中
        }
        ui->ispDowPortName->setModel(model); // 下拉框设置model
    }

    connect(serialPortInfo, &SerialPortInfo::update, this, [=](const QStringList &ports) {
        // 先移除再添加
        ui->ispDowPortName->clear();
        if (!ports.empty()) {
            ui->ispDowPortName->addItems(ports);
            auto *model = new QStandardItemModel();
            for (int i = 0; i < ui->ispDowPortName->count(); i++) {
                auto *item = new QStandardItem(ui->ispDowPortName->itemText(i));
                item->setToolTip(ui->ispDowPortName->itemText(i)); // 设置提示文本
                model->appendRow(item);                            // 将item添加到model中
            }
            ui->ispDowPortName->setModel(model); // 下拉框设置model
        }
    });

    /*************调整下拉控件样式*******************************/
    ui->ispDowBaudRate->setView(new QListView(ui->ispDowBaudRate));
    ui->ispDowHWBtn->setView(new QListView(ui->ispDowHWBtn));
    ui->ispDowPortName->setView(new QListView(ui->ispDowPortName));

    /*****************ISP界面按钮信号与槽连接*************************************/
    connect(ui->ispDowOpBtn, &QPushButton::clicked, this,
            [=]() // 串口操作按钮
    {
        dowCmdArg.clear();
        if (!ispConnected) // 串口关闭需要打开
        {
            dowCmdArg.append(ui->ispDowPortName->currentText());                  // 端口名
            dowCmdArg.append(ui->ispDowBaudRate->currentText());                  // 波特率
            dowCmdArg.append(QString("%1").arg(ui->ispDowHWBtn->currentIndex())); // 流控
            this->dowPackCmd(DOW::TYPE::ISP, DOW::CMD::Open, dowCmdArg);
        } else // 串口已经打开需要关闭
        {
            ispConnected = false;
            ui->ispDowOpBtn->setText(tr("打开串口"));
            ui->ispDowReadInfoBtn->setEnabled(false);
            ui->ispDowFlashBtn->setEnabled(false);
            ui->ispDowEraseBtn->setEnabled(false);
            if (iapProcessIng) {
                emit closeDowThread();
            }
            this->dowPackCmd(DOW::TYPE::ISP, DOW::CMD::Close, dowCmdArg);
        }
    });

    // 烧录按钮
    connect(ui->ispDowFlashBtn, &QPushButton::clicked, this, [=]() {
        if (!ui->fwInfo->item(0, 0)->text().isEmpty()) {
            dowCmdArg.clear();
            dowCmdArg << ui->fwInfo->item(0, 0)->text();
            if (dowCmdArg[0].endsWith(".hex", Qt::CaseInsensitive)) {
                // 转换之后的文件名字加上起始地址
                dowCmdArg[0].replace(".hex",
                                     QString("_%1.bin").append(ui->fwInfo->item(0, 1)->text()),
                                     Qt::CaseInsensitive);
            }
            ui->dowProgressBar->setValue(0);
            this->dowPackCmd(DOW::TYPE::ISP, DOW::Program, dowCmdArg);
        } else {
            QMessageBox::critical(this, tr("错误"), tr("请选择固件"));
        }
    });

    // 读取芯片信息按钮
    connect(ui->ispDowReadInfoBtn, &QPushButton::clicked, this, [=]() {
        dowCmdArg.clear();
        this->dowPackCmd(DOW::TYPE::ISP, DOW::ReadInfo, dowCmdArg);
    });

    // 擦除按钮
    connect(ui->ispDowEraseBtn, &QPushButton::clicked, this, [=]() {
        dowCmdArg.clear();
        this->dowPackCmd(DOW::TYPE::ISP, DOW::Erase, dowCmdArg);
    });

    /**************IAP界面*****************************/
    ui->iapDowBaudRate->setCurrentIndex(6);

    ui->iapDowReadInfoBtn->setEnabled(false);
    ui->iapDowFlashBtn->setEnabled(false);
    ui->iapDowEraseBtn->setEnabled(false);

    // 刷新可用串口
    auto iap_ports = SerialPortInfo::availablePorts();
    ui->iapDowPortName->addItems(iap_ports);

    if (!iap_ports.empty()) {
        ui->iapDowPortName->addItems(iap_ports);
        auto *model = new QStandardItemModel();
        for (int i = 0; i < ui->iapDowPortName->count(); i++) {
            auto *item = new QStandardItem(ui->iapDowPortName->itemText(i));
            item->setToolTip(ui->iapDowPortName->itemText(i)); // 设置提示文本
            model->appendRow(item);                            // 将item添加到model中
        }
        ui->iapDowPortName->setModel(model); // 下拉框设置model
    }

    connect(serialPortInfo, &SerialPortInfo::update, this, [=](const QStringList &ports) {
        ui->iapDowPortName->clear();
        if (!ports.empty()) {
            ui->iapDowPortName->addItems(ports);
            auto *model = new QStandardItemModel();
            for (int i = 0; i < ui->iapDowPortName->count(); i++) {
                auto *item = new QStandardItem(ui->iapDowPortName->itemText(i));
                item->setToolTip(ui->iapDowPortName->itemText(i)); // 设置提示文本
                model->appendRow(item);                            // 将item添加到model中
            }
            ui->iapDowPortName->setModel(model); // 下拉框设置model
        }
    });

    /*************调整下拉控件样式*******************************/
    ui->iapDowBaudRate->setView(new QListView(ui->iapDowBaudRate));
    ui->iapDowPortName->setView(new QListView(ui->iapDowPortName));

    /*****************IAP界面按钮信号与槽连接*************************************/

    // 串口操作按钮
    connect(ui->iapDowOpBtn, &QPushButton::clicked, this, [=]() {
        dowCmdArg.clear();
        if (!iapConnected) // 串口关闭需要打开
        {
            dowCmdArg.append(ui->iapDowPortName->currentText()); // 端口名
            dowCmdArg.append(ui->iapDowBaudRate->currentText()); // 波特率
            this->dowPackCmd(DOW::TYPE::IAP, DOW::CMD::Open, dowCmdArg);
        } else // 串口已经打开需要关闭
        {
            iapConnected = false;
            ui->iapDowOpBtn->setText(tr("打开串口"));
            ui->iapDowReadInfoBtn->setEnabled(false);
            ui->iapDowFlashBtn->setEnabled(false);
            ui->iapDowEraseBtn->setEnabled(false);
            if (iapProcessIng) {
                emit closeDowThread();
            }
            this->dowPackCmd(DOW::TYPE::IAP, DOW::CMD::Close, dowCmdArg);
        }
    });

    // 烧录按钮
    connect(ui->iapDowFlashBtn, &QPushButton::clicked, this, [=]() {
        if (!ui->fwInfo->item(0, 0)->text().isEmpty()) {
            dowCmdArg.clear();
            dowCmdArg << ui->fwInfo->item(0, 0)->text();
            ui->dowProgressBar->setValue(0);
            this->dowPackCmd(DOW::TYPE::IAP, DOW::Program, dowCmdArg);
        } else {
            QMessageBox::critical(this, tr("错误"), tr("请选择固件"));
        }
    });

    // 读取芯片信息按钮
    connect(ui->iapDowReadInfoBtn, &QPushButton::clicked, this, [=]() {
        dowCmdArg.clear();
        this->dowPackCmd(DOW::TYPE::IAP, DOW::ReadInfo, dowCmdArg);
    });

    // 擦除按钮
    connect(ui->iapDowEraseBtn, &QPushButton::clicked, this, [=]() {
        dowCmdArg.clear();
        this->dowPackCmd(DOW::TYPE::IAP, DOW::Erase, dowCmdArg);
    });
}

/**
 * @brief 打包下载命令
 * @param type 下载方式
 * @param cmd 命令
 * @param arg 命令参数
 */
void Widget::dowPackCmd(DOW::TYPE type, DOW::CMD cmd, QStringList &arg)
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
void Widget::dowMsgHandle(const DOW::TYPE &type, const COMMON_MSG::MSG &msg, const QStringList &arg)
{
    switch (msg) {
    case COMMON_MSG::MSG::OpenFail:
        qDebug() << "OpenFail";
        if (type == DOW::ISP) {
            ispConnected = false;
        } else {
            iapConnected = false;
        }
        QMessageBox::warning(this, tr("警告"), tr("无法打开串口"));
        break;
    case COMMON_MSG::MSG::OpenSuccessful:
        qDebug() << "OpenSuccessful" << type;
        if (type == DOW::TYPE::ISP) {
            ispConnected = true;
            ui->ispDowReadInfoBtn->setEnabled(true);
            ui->ispDowFlashBtn->setEnabled(true);
            ui->ispDowEraseBtn->setEnabled(true);
            ui->ispDowOpBtn->setText(tr("关闭串口"));
        } else if (type == DOW::TYPE::IAP) {
            iapConnected = true;
            ui->iapDowReadInfoBtn->setEnabled(true);
            ui->iapDowFlashBtn->setEnabled(true);
            ui->iapDowEraseBtn->setEnabled(true);
            ui->iapDowOpBtn->setText(tr("关闭串口"));
        }
        break;
    case COMMON_MSG::ProcessIng:
        if (type == DOW::ISP) {
            ispProcessIng = true;
        } else if (type == DOW::IAP) {
            setDowUiIsEnabled(DOW::TYPE::IAP, false);
            iapProcessIng = true;
        }
        break;
    case COMMON_MSG::ProcessDone:
        if (type == DOW::ISP) {
            ispProcessIng = false;
        } else if (type == DOW::IAP) {
            setDowUiIsEnabled(DOW::TYPE::IAP, true);
            iapProcessIng = false;
        }
        break;
    case COMMON_MSG::MSG::Log:
        for (const auto &str : arg) {
            ui->dowLogText->moveCursor(QTextCursor::End); // 移动光标到末尾
            ui->dowLogText->insertPlainText(str);         // 文末追加文本
        }
        break;
    case COMMON_MSG::MSG::Progress:
        ui->dowProgressBar->setValue(arg[0].toInt());
    default:
        break;
    }
}

/**
 * @brief 设置下载界面控件是否可用
 * @param stste 状态
 */
void Widget::setDowUiIsEnabled(DOW::TYPE type, bool stste)
{
    if (type == DOW::TYPE::ISP) {
        ui->ispDowOpBtn->setEnabled(stste);
        ui->ispDowFlashBtn->setEnabled(stste);
        ui->ispDowEraseBtn->setEnabled(stste);
        ui->ispDowReadInfoBtn->setEnabled(stste);
    } else if (type == DOW::TYPE::IAP) {
        ui->iapDowOpBtn->setEnabled(stste);
        ui->iapDowFlashBtn->setEnabled(stste);
        ui->iapDowEraseBtn->setEnabled(stste);
        ui->iapDowReadInfoBtn->setEnabled(stste);
    }
}

/**
 * @brief hex字符串转为ascii字符串
 * @param source 源文本
 * @return 转换后的文本
 */
void Widget::HexQString_to_QString(QString &source)
{
    QByteArray temp;
    temp = source.remove(" ").toLatin1();

    // 将temp中的16进制字符串转化为对应的字符串，比如"31"转换为"1"
    source = QByteArray::fromHex(temp);
}

/**
 * @brief ascii字节流转hex字节流
 * @param source 源文本
 * @return 换后的文本
 */
void Widget::QByteArray_to_HexQByteArray(QByteArray &source)
{
    source = source.toHex();
    int length = source.length();
    for (int i = 0; i <= (length / 2) - 1; ++i) {
        source.insert((3 * i + 2), ' '); // 插入空格字符串便于区分一个字节
    }
}

/**
 * @brief 加载样式表
 * @param styleSheetFile 样式表文件路径
 */
void Widget::loadStyleSheet(const QString &styleSheetFile)
{
    QFile file(styleSheetFile);
    file.open(QFile::ReadOnly);
    if (file.isOpen()) {
        QString styleSheet = this->styleSheet();
        styleSheet += QLatin1String(file.readAll()); // 读取样式表文件
        this->setStyleSheet(styleSheet);
        file.close();
    }
}

/**
 * @brief 鼠标进入事件
 * @param event 事件
 */
void Widget::enterEvent(QEnterEvent *event)
{
    setCursor(Qt::ArrowCursor);
    QApplication::restoreOverrideCursor(); // 恢复鼠标指针性状
    QWidget::enterEvent(event);
}

/**
 * @brief 事件过滤器，过滤鼠标移动事件，防止鼠标移动事件传递到父控件
 * @param target 过滤对象
 * @param event 事件
 * @return 是否传递事件
 */
bool Widget::eventFilter(QObject *target, QEvent *event)
{
    if (event->type() == QEvent::MouseMove && target == this) {
        return true;
    } else {
        return false;
    }
}
