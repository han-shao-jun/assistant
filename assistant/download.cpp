#include "download.h"
#include <QFile>
#include <QFileInfo>


QQueue<QByteArray> recCopy;        //接收到的原始信息
QQueue<QByteArray> recBuff;        //接收到的原始信息

Download::Download(QObject *parent) : QObject(parent)
{

}

Download::~Download()
{
    if (isConnected)
    {
        isConnected = false;
        port->close();
        delete port;
        port = nullptr;
    }
}


void Download::endSeddion()
{
    if (!session_done)
    {
        session_done = true;
    }
}


/**
 * @brief 接收主线程下载通信配置
 * @param config 配置字符串列表
 */
void Download::doWork(const QStringList& config)
{
    qDebug() << config;
    qDebug() << "doWork id: " << QThread::currentThreadId();

    //通信协议
    type = config[0].toUInt();
    cmd = config[1].toUInt();

    if (type == DOW::TYPE::ISP) //ISP串口下载
    {
//        flowControl = config[2];
//        port->setPortName(config[3].section(':', 0, 0));
//        port->setBaudRate(config[4].toInt());        //波特率
//        port->setParity(QSerialPort::EvenParity);    //校验位
//        port->setDataBits(QSerialPort::Data8);       //数据位
//        port->setStopBits(QSerialPort::OneStop);     //停止位
//        port->setFlowControl(QSerialPort::NoFlowControl);
//        if (!port->open(QIODevice::ReadWrite))
//        {
//            isConnected = false;
//            emit msgSignal("openFail");
//        }
//        else
//        {
//            isConnected = true;
//            emit msgSignal("openSuccessful");
//        }
    }
    else if (type == DOW::TYPE::IAP)
    {
        if (cmd == DOW::CMD::Open)
        {
            qDebug() << "open";

            if (port == nullptr)
            {
                port = new QSerialPort();
                port->setPortName(config[2].section(':', 0, 0));
                port->setBaudRate(config[3].toInt());        //波特率
                port->setParity(QSerialPort::NoParity);      //校验位
                port->setDataBits(QSerialPort::Data8);       //数据位
                port->setStopBits(QSerialPort::OneStop);     //停止位
                port->setFlowControl(QSerialPort::NoFlowControl);
                if (!isConnected)
                {
                    if (!port->open(QIODevice::ReadWrite))
                    {
                        isConnected = false;
                        delete port;
                        port = nullptr;
                        arg.clear();
                        arg.append(QString("OpenFail"));
                        emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::OpenFail, arg);
                    }
                    else
                    {
                        isConnected = true;
                        arg.clear();
                        arg.append(QString("OpenSuccessful"));
                        emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::OpenSuccessful, arg);
                    }
                }
            }
        }
        else if (cmd == DOW::CMD::Flash)
        {
            qDebug() << "Flash";
            if (isConnected)
            {
                quint8 state = BEGIN;
                quint8 pktNo = 0;
                quint8 time_out_cnt = 0;
                qint64 file_size;
                QFileInfo fileInfo(config[2]);
                QByteArray data;
                QByteArray SourceBuf;
                quint32 data_size = 0;
                QFile file(config[2]);
                session_done = false;

                arg.clear();
                emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::ProcessIng, arg);

                file_size = fileInfo.size();
                arg.clear();
                arg.append(QString("IAP:固件大小%1字节\r\n").arg(file_size));
                arg.append(QString("IAP:预计发送%1个固件数据包\r\n").arg((file_size/PACKET_1K_SIZE) + !(!(file_size%PACKET_1K_SIZE))));
                emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);
                if (file.open(QIODevice::ReadOnly))
                {
                    while (!session_done)
                    {
                        if (port->waitForReadyRead(150))
                        {
                            time_out_cnt = 0;
                            QByteArray rec = port->readAll();

                            qDebug() << "rec" << rec;
                            if (rec.at(0) == ABORT1 || rec.at(0) == ABORT2)
                            {
                                session_done = true;
                            }
                            else
                            {
                                switch (state)
                                {
                                case BEGIN:
                                    if (rec.size() == 1 && rec.at(0) == CRC16)   //收到字符“C"
                                    {
                                        state = START;
                                    }
                                    break;
                                case START:
                                    if (rec.at(0) == NAK) //收到NAK和字符“C"，重发
                                    {
                                        state = START;
                                    }
                                    else if (rec.at(0) == ACK) //收到ACK和字符“C"，转移到发送数据包
                                    {
                                        state = SEND;
                                    }
                                    break;
                                case SEND:
                                    if (file_size == 0)
                                    {
                                        state = END_CHECK;
                                    }
                                    break;
                                case END_CHECK:
                                    if (rec.at(0) == ACK)
                                    {
                                        state = END;
                                    }
                                    break;
                                case END:
                                    session_done = true;
                                    break;
                                default:
                                    break;
                                }

                                data.clear();
                                arg.clear();
                                switch (state)
                                {
                                case START:
                                    Ymodem::PrepareIntialPacket(data, fileInfo.fileName().toStdString().data(), file_size);
                                    arg.append(QString("IAP:发送固件信息包\r\n"));
                                    arg.append(QString("IAP:等待擦除FLASH\r\n"));
                                    emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);
                                    break;
                                case SEND:
                                    if (rec.at(0) == ACK)   //应答
                                    {
                                        if (file_size >= PACKET_1K_SIZE)    //剩余数据大于等于1024字节
                                        {
                                            SourceBuf = file.read(PACKET_1K_SIZE);
                                            data_size = PACKET_1K_SIZE;
                                            pktNo++;
                                            file_size -= PACKET_1K_SIZE;
                                        }
                                        else if (file_size != 0)    //还有剩余数据但小于1024字节
                                        {
                                            SourceBuf = file.read(file_size);
                                            data_size = file_size;
                                            pktNo++;
                                            file_size = 0;
                                        }
                                        arg.append(QString("IAP:发送第%1个固件数据包\r\n").arg(pktNo));
                                        emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);
                                    }
                                    else
                                    {
                                        arg.append(QString("IAP:第%1个固件数据包重发\r\n").arg(pktNo));
                                        emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);
                                    }
                                    Ymodem::PrepareDataPacket(SourceBuf, data, pktNo, data_size);
                                    break;
                                case END_CHECK:
                                    data.append(EOT);
                                    arg.append(QString("IAP:发送结束确认包\r\n"));
                                    emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);
                                    break;
                                case END:
                                    Ymodem::PrepareEndPacket(data);
                                    arg.append(QString("IAP:发送结束包\r\n"));
                                    arg.append(QString("IAP:下载完成\r\n"));
                                    emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);
                                    session_done = true;
                                    break;
                                default:
                                    break;
                                }
                                if (state != BEGIN)
                                {
                                    port->write(data);
                                    port->flush();      //刷新下缓冲区，使串口立即发送
                                    port->waitForBytesWritten(1);
                                }
                            }
                        }
                        else
                        {
                            if ((state != BEGIN) && (state != START))
                            {
                                time_out_cnt++;
                                if (time_out_cnt == 5)
                                {
                                    session_done = true;
                                }
                                arg.clear();
                                arg.append(QString("IAP:超时，未收到任何反馈\r\n"));
                                emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);
                            }
                        }
                    }
                    arg.clear();
                    arg.append(QString("IAP:ProcessDone\r\n"));
                    emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::ProcessDone, arg);
                }
                else
                {
                    arg.clear();
                    arg.append(QString("IAP:打开固件失败\r\n"));
                    emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::Log, arg);
                }
            }
        }
        else if (cmd == DOW::CMD::ReadInfo)
        {
            qDebug() << "ReadInfo";
            if (isConnected)
            {
                port->write("ReadInfo");
                port->flush();  //刷新下缓冲区，使串口立即发送
                port->waitForBytesWritten(10);
            }
        }
        else if (cmd == DOW::CMD::Erase)
        {
            qDebug() << "Erase";
            if (isConnected)
            {
                port->write("Erase");
                port->flush();  //刷新下缓冲区，使串口立即发送
                port->waitForBytesWritten(10);
            }
        }
        else if (cmd == DOW::CMD::Close)
        {
            qDebug() << "Close";
            if (isConnected)
            {
                isConnected = false;
                port->close();
                delete port;
                port = nullptr;
            }
        }
    }
}







