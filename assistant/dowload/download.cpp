#include "download.h"

QSerialPort *gPort = nullptr;

Download::Download(QObject *parent) : QObject(parent) {}

Download::~Download()
{
    if (isConnected) {
        isConnected = false;
        port->close();
        delete port;
        port = nullptr;
    }
}

void Download::endSession()
{
    if (!sessionDone) {
        sessionDone = true;
    }
}

/**
 * @brief 根据主线程配置执行主线程的命令
 * @param config 配置字符串列表
 */
void Download::doWork(const QStringList &config)
{
    qDebug() << config;
    qDebug() << "doWork id: " << QThread::currentThreadId();

    // 通信协议
    type = config[0].toUInt();
    cmd = config[1].toUInt();

    if (type == DOW::TYPE::ISP) // ISP串口下载
    {
        switch (cmd) {
        case DOW::CMD::Open:
            qDebug() << "IspOpen";
            this->IspOpen(config[2].section(':', 0, 0), config[3].toInt());
            break;
        case DOW::CMD::ReadInfo:
            qDebug() << "IspReadInfo";
            this->IspReadInfo();
            break;
        case DOW::CMD::Erase:
            qDebug() << "IspErase";
            this->IspErase();
            break;
        case DOW::CMD::Program:
            qDebug() << "Program";
            /**************先擦除**************/
            this->IspErase();

            /**************再烧录**************/
            this->IspFlash(config[2]);

            /**************再执行**************/
            this->IspExecute(0x08000000);
            break;
        case DOW::CMD::Execute:
            qDebug() << "Execute";
            this->IspExecute(0x08000000);
            break;
        case DOW::CMD::Close:
            qDebug() << "Close";
            port->close();
            delete port;
            port = nullptr;
            gPort = nullptr;
        default:
            break;
        }
    } else if (type == DOW::TYPE::IAP) {
        if (cmd == DOW::CMD::Open) {
            qDebug() << "open";

            this->IapOpen(config[2].section(':', 0, 0), config[3].toInt());
        } else if (cmd == DOW::CMD::Program) {
            qDebug() << "Program";

            this->IapFlash(config[2]);
        } else if (cmd == DOW::CMD::ReadInfo) {
            qDebug() << "ReadInfo";

            port->write("ReadInfo");
            port->flush(); // 刷新下缓冲区，使串口立即发送
            port->waitForBytesWritten(10);
        } else if (cmd == DOW::CMD::Erase) {
            qDebug() << "Erase";

            port->write("Erase");
            port->flush(); // 刷新下缓冲区，使串口立即发送
            port->waitForBytesWritten(10);
        } else if (cmd == DOW::CMD::Close) {
            qDebug() << "Close";

            isConnected = false;
            port->close();
            delete port;
            port = nullptr;
        }
    }
}

/**
 * @brief 打开串口
 * @param portName 串口端口号
 * @param baudRate 波特率
 * @return true    打开串口成功
 * @return false   打开串口失败
 */
bool Download::OpenSerialPort(const QString &portName, const int &baudRate)
{
    port->setPortName(portName);                      // 串口号
    port->setBaudRate(baudRate);                      // 波特率
    port->setParity(QSerialPort::EvenParity);         // 校验位
    port->setDataBits(QSerialPort::Data8);            // 数据位
    port->setStopBits(QSerialPort::OneStop);          // 停止位
    port->setFlowControl(QSerialPort::NoFlowControl); // 流控

    return port->open(QIODevice::ReadWrite);
}

/**
 * @brief ISP打开串口
 * @param portName 串口端口号
 * @param baudRate 波特率
 */
void Download::IspOpen(const QString &portName, const int &baudRate)
{
    port = new QSerialPort();
    gPort = port;
    if (!this->OpenSerialPort(portName, baudRate)) {
        port->close();
    } else {
        // 开始连接
        qDebug() << "connecting...";
        arg.clear();
        arg.append(tr("[ISP] 连接中."));
        emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::Log, arg);

        int try_cnt = 10;
        while (try_cnt) {
            if (ISP_Sync()) {
                isConnected = true;
                arg.clear();
                arg.append(QString("OpenSuccessful"));
                emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::OpenSuccessful, arg);

                arg.clear();
                arg.append(tr(" 连接成功！！！\r\n"));
                emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::Log, arg);
                return;
            } else {
                arg.clear();
                arg.append(tr("."));
                emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::Log, arg);
                try_cnt--;
            }
        }
        arg.clear();
        arg.append(tr("\r\n"));
        emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::Log, arg);

        port->close();
        isConnected = false;
        delete port;
        port = nullptr;
        gPort = nullptr;

        arg.clear();
        arg.append(QString("OpenFail"));
        emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::OpenFail, arg);
    }
}

/**
 * @brief ISP读取BootLoader信息
 */
void Download::IspReadInfo()
{
    uint8_t recArray[4];

    arg.clear();
    if (ISP_GetVersion(recArray)) {
        arg.append(
            tr("[ISP] BootLoader 版本: %1.%2\r\n").arg(recArray[0] >> 4).arg(recArray[0] & 0x0f));
    } else {
        arg.append(tr("[ISP] 获取BootLoader版本失败\r\n"));
    }
    emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::Log, arg);

    arg.clear();
    if (ISP_GetPID(recArray)) {
        arg.append(tr("[ISP] PID: 0x%1%2\r\n")
                       .arg(recArray[1], 2, 16, QLatin1Char('0'))
                       .arg(recArray[2], 2, 16, QLatin1Char('0')));
    } else {
        arg.append(tr("[ISP] 获取PID失败\r\n"));
    }
    emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::Log, arg);
}

/**
 * @brief ISP全片擦除flash
 */
void Download::IspErase()
{
    arg.clear();
    arg.append(tr("[ISP] 开始全片擦除，全片擦除时间比较长，请耐心等候\r\n"));
    emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::Log, arg);

    arg.clear();
    if (ISP_Erase()) {
        arg.append(tr("[ISP] 全片擦除成功\r\n"));
        emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::Log, arg);
    } else {
        arg.append(tr("[ISP] 擦除命令无应答\r\n"));
        emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::Log, arg);
    }
}

/**
 * @brief ISP烧录程序
 * @param filePath 固件的文件路径
 */
void Download::IspFlash(const QString &filePath)
{
    qint64 fileSize;
    QFile file(filePath);
    uint32_t pktNum, pktNo = 0;
    uint32_t baseAddr = 0x08000000;
    uint8_t buffer[256];
    uint32_t dataSize;
    QByteArray sourceBuf;

    if (!file.open(QIODevice::ReadOnly))
        return;
    fileSize = file.size();
    pktNum = (fileSize / 256) + !(!(fileSize % 256));
    arg.clear();
    arg.append(tr("[ISP] 固件大小%1字节(%2K字节)\r\n").arg(fileSize).arg(fileSize / 1024));
    arg.append(tr("[ISP] 预计发送%1个固件数据包\r\n").arg(pktNum));
    emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);

    arg.clear();
    emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::ProcessIng, arg);
    while (true) {
        /*********剩余数据量大于等于256字节 ******/
        if (fileSize >= 256) {
            dataSize = 256;
            fileSize -= 256;
        } else if (fileSize != 0) {
            dataSize = fileSize;
        }

        /********从文件中容获取数据**********/
        sourceBuf = file.read(dataSize);
        memcpy(buffer, sourceBuf.data(), dataSize);

        /********烧录是否成功？**********/
        if (!ISP_ProgramFlash(baseAddr, buffer, dataSize)) {
            arg.append(tr("[ISP] 烧录命令无应答\r\n"));
            emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::Log, arg);
            return;
        } else {
            /**************显示进度***********/
            baseAddr += dataSize;
            pktNo++;
            arg.clear();
            arg.append(QString("%1").arg((pktNo * 100) / pktNum));
            emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::Progress, arg);
            if (pktNo == pktNum)
                break;
        }
    }

    arg.clear();
    emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::ProcessDone, arg);
}

/**
 * @brief ISP执行程序
 * @param addr 程序起始地址
 */
void Download::IspExecute(uint32_t addr)
{
    arg.clear();
    if (ISP_Go(addr)) {
        arg.append(tr("[ISP] 程序执行成功\r\n"));
        emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::Log, arg);
    } else {
        arg.append(tr("[ISP] 程序执行成功\r\n"));
        emit msgSignal(DOW::TYPE::ISP, COMMON_MSG::MSG::Log, arg);
    }
}

/**
 * @brief IAP打开串口
 * @param portName 串口端口号
 * @param baudRate 波特率
 */
void Download::IapOpen(const QString &portName, const int &baudRate)
{
    port = new QSerialPort();
    if (!OpenSerialPort(portName, baudRate)) {
        isConnected = false;
        delete port;
        port = nullptr;
        arg.clear();
        arg.append(QString("OpenFail"));
        emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::OpenFail, arg);
    } else {
        isConnected = true;
        arg.clear();
        arg.append(QString("OpenSuccessful"));
        emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::OpenSuccessful, arg);
    }
}

/**
 * @brief IAP烧录程序
 * @param filePath 固件的文件路径
 */
void Download::IapFlash(const QString &filePath)
{
    quint8 state = BEGIN; // IAP流程阶段
    quint8 pktNo = 0;
    quint8 timeOutCnt = 0;
    qint64 fileSize;
    QFileInfo fileInfo(filePath);
    QByteArray data;
    uint8_t dataArray[PACKET_1K_SIZE + PACKET_OVERHEAD];
    QByteArray sourceBuf;
    uint8_t sourceBufArray[PACKET_1K_SIZE];
    quint32 dataSize = 0;
    QFile file(filePath);
    sessionDone = false;
    quint32 pktNum;
    arg.clear();
    emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::ProcessIng, arg);

    /*********计算总共发送数据数量***************/
    fileSize = fileInfo.size();
    pktNum = (fileSize / PACKET_1K_SIZE) + !(!(fileSize % PACKET_1K_SIZE));

    arg.clear();
    arg.append(tr("[IAP] 固件大小%1字节\r\n").arg(fileSize));
    arg.append(tr("[IAP] 预计发送%1个固件数据包\r\n").arg(pktNum));
    emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);

    if (file.open(QIODevice::ReadOnly)) {
        while (!sessionDone) {
            if (port->waitForReadyRead(200)) {
                timeOutCnt = 0;
                QByteArray rec = port->readAll();

                qDebug() << "rec" << rec;
                if (rec.at(0) == ABORT1 || rec.at(0) == ABORT2) {
                    sessionDone = true;
                } else { // 根据上一次的IAP流程阶段决定当前所处那个流程阶段
                    switch (state) {
                    case BEGIN:
                        if (rec.at(0) == CRC16) // 收到字符“C"
                        {
                            state = START;
                        }
                        break;
                    case START:
                        if (rec.at(0) == NAK) // 收到NAK和字符“C"，重发
                        {
                            state = START;
                            qDebug() << "NAK";
                        } else if (rec.at(0) == ACK) // 收到ACK和字符“C"，转移到发送数据包
                        {
                            state = SEND;
                            arg.clear();
                            arg.append(QString("[IAP] "));
                            emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);
                        }
                        break;
                    case SEND:
                        if (fileSize == 0) // 固件发送完成
                        {
                            state = END_CHECK;
                            arg.clear();
                            arg.append(QString(">\r\n"));
                            emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);
                        }
                        break;
                    case END_CHECK:
                        if (rec.at(0) == ACK && rec.at(1) == CRC16) {
                            state = END;
                        }
                        break;
                    case END:
                        sessionDone = true;
                        break;
                    default:
                        break;
                    }

                    data.clear();
                    arg.clear();

                    /*********准备串口要发送数据包***********/
                    switch (state) {
                    case START: // 固件信息包
                        Ymodem_PrepareIntialPacket(
                            dataArray, fileInfo.fileName().toStdString().data(), fileSize);
                        data.resize(PACKET_OVERHEAD + PACKET_SIZE);
                        memcpy(data.data(), dataArray,
                               PACKET_OVERHEAD + PACKET_SIZE); // copy数据到QByteArray容器中
                        arg.append(tr("[IAP] 发送固件信息包\r\n"));
                        arg.append(tr("[IAP] 等待擦除FLASH\r\n"));
                        emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);
                        break;
                    case SEND:                // 固件数据包
                        if (rec.at(0) == ACK) // 应答
                        {
                            if (fileSize >= PACKET_1K_SIZE) // 剩余数据大于等于1024字节
                            {
                                sourceBuf = file.read(PACKET_1K_SIZE);
                                dataSize = PACKET_1K_SIZE;
                                pktNo++;
                                fileSize -= PACKET_1K_SIZE;
                            } else if (fileSize != 0) // 还有剩余数据但小于1024字节
                            {
                                sourceBuf = file.read(fileSize);
                                dataSize = fileSize;
                                pktNo++;
                                fileSize = 0;
                            }
                            // 显示log文本
                            arg.append(QString("-"));
                            emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);

                            // 显示进度
                            arg.clear();
                            arg.append(QString("%1").arg((pktNo * 100) / pktNum));
                            emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Progress, arg);
                        } else {
                            arg.append(tr("[IAP] 第%1个固件数据包重发\r\n").arg(pktNo));
                            emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);
                        }
                        memcpy(sourceBufArray, sourceBuf.data(),
                               dataSize); // 从QByteArray容器获取数据到
                        Ymodem_PrepareDataPacket(sourceBufArray, dataArray, pktNo, dataSize);
                        if (dataArray[0] == STX) {
                            data.resize(PACKET_OVERHEAD + PACKET_1K_SIZE);
                            memcpy(data.data(), dataArray,
                                   PACKET_OVERHEAD + PACKET_1K_SIZE); // copy数据到QByteArray容器中
                            qDebug() << "STX" << dataArray[0];
                        } else {
                            data.resize(PACKET_OVERHEAD + PACKET_SIZE);
                            memcpy(data.data(), dataArray,
                                   PACKET_OVERHEAD + PACKET_SIZE); // copy数据到QByteArray容器中
                        }
                        break;
                    case END_CHECK:
                        data.append(EOT);
                        arg.append(tr("[IAP] 发送结束确认包\r\n"));
                        emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);
                        break;
                    case END:
                        Ymodem_PrepareEndPacket(dataArray);
                        data.resize(PACKET_OVERHEAD + PACKET_SIZE);
                        memcpy(data.data(), dataArray,
                               PACKET_OVERHEAD + PACKET_SIZE); // copy数据到QByteArray容器中
                        arg.append(tr("[IAP] 发送结束包\r\n"));
                        arg.append(tr("[IAP] 下载完成\r\n"));
                        emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);
                        sessionDone = true;
                        break;
                    default:
                        break;
                    }
                    if (state != BEGIN) {
                        port->write(data);
                        port->flush(); // 刷新下缓冲区，使串口立即发送
                        port->waitForBytesWritten(10);
                    }
                }
            } else {
                if ((state != BEGIN) && (state != START)) {
                    timeOutCnt++;
                    if (timeOutCnt == 5) {
                        sessionDone = true;
                    }
                    arg.clear();
                    arg.append(tr("[IAP] 超时，未收到任何反馈\r\n"));
                    emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);
                }
            }
        }
        arg.clear();
        arg.append(QString("[IAP] ProcessDone\r\n"));
        emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::ProcessDone, arg);
    } else {
        arg.clear();
        arg.append(tr("[IAP] 打开固件失败\r\n"));
        emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);
    }
}

/**
 * @brief 串口发送给数据
 * @param buffer 待发送数据存储区
 * @param len    数据长度(byte)
 * @param timeOut 等待发送时长
 * @return 已经发送的的长度(byte)
 */
extern "C" bool SeriPortWrite(uint8_t *buffer, uint32_t len, uint32_t timeOut)
{
    QByteArray data;

    assert(buffer != nullptr && len != 0);

    data.resize((int)len);
    memcpy(data.data(), buffer, len);
    gPort->write(data);
    gPort->flush();
    return gPort->waitForBytesWritten((int)timeOut);
}

/**
 * @brief 串口接收数据
 * @param buffer 数据存储区
 * @param len    数据长度(byte)
 * @param timeOut 等待发送时长
 * @return 已经接收的的长度(byte)
 */
extern "C" uint32_t SeriPortRead(uint8_t *buffer, uint32_t len, uint32_t timeOut)
{
    char rec[256];
    qint64 lenTemp;

    assert(buffer != nullptr);

    gPort->waitForReadyRead((int)timeOut);
    lenTemp = gPort->read(rec, len);
    memcpy(buffer, rec, lenTemp);
    return lenTemp;
}

/**
 * @brief STM32进入ISP模式
 */
extern "C" void STM32_GoIntoIsp()
{
    gPort->setDataTerminalReady(false); // DTR电平置低,复位
    gPort->setRequestToSend(true);      // RTS置高,选择进入BootLoader
    QThread::msleep(100);               // 延时100毫秒
    gPort->setDataTerminalReady(true);  // DTR电平变高释放复位
}
