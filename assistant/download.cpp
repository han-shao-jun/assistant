#include "download.h"
#include <QFile>
#include <QFileInfo>


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
                port->setParity(QSerialPort::EvenParity);    //校验位
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
                        emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::OpenFail);
                    }
                    else
                    {
                        isConnected = true;
                        emit msgSignal(DOW::TYPE::IAP, COMMON_MSG::MSG::OpenSuccessful);
                    }
                }
            }
        }
        else if (cmd == DOW::CMD::Flash)
        {
            qDebug() << "Flash";
            if (isConnected)
            {
                quint8 state = 0;
                quint8 pktNo = 0;
                quint8 time_out_cnt = 0;
                qint64 file_size;
                quint8 end_cnt = 0;
                QFileInfo fileInfo(config[2]);
                QByteArray data;
                QByteArray SourceBuf;
                QFile file(config[2]);
                bool session_done = false;

                file_size = fileInfo.size();
                if (file.open(QIODevice::ReadOnly))
                {
                    while (!session_done)
                    {
                        if (port->waitForReadyRead())
                        {
                            time_out_cnt = 0;
                            QByteArray rec = port->readAll();

                            if (rec.at(0) == ABORT1 || rec.at(0) == ABORT2)
                            {
                                qDebug() << "abort by user";
                                break;
                            }
                            else
                            {
                                switch (state)
                                {
                                case START:
                                    if (rec.at(0) == 'C')   //收到字符“C"
                                    {
                                        data.clear();
                                        Ymodem::PrepareIntialPacket(data, fileInfo.fileName().toStdString().data(), file_size);
                                        port->write(data);
                                        port->flush();  //刷新下缓冲区，使串口立即发送
                                        port->waitForBytesWritten(1);
                                        qDebug() << "START" << pktNo;
                                        pktNo++;
                                        state = SEND;
                                    }
                                    break;
                                case FIRST_SEND:
                                    if (rec.size() == 2)
                                    {
                                        if (rec.at(0) == ACK && rec.at(1) == 'C')
                                        {
                                            data.clear();
                                            if (file_size >= PACKET_1K_SIZE)    //大于等于1024
                                            {
                                                SourceBuf = file.read(PACKET_1K_SIZE);
                                                Ymodem::PrepareDataPacket(SourceBuf, data, pktNo, PACKET_1K_SIZE);
                                                file_size -= PACKET_1K_SIZE;
                                                if (file_size == 0) //等于1024
                                                {
                                                    state = END_CHECK;
                                                }
                                            }
                                            else    //小于1024,一包可以发完
                                            {
                                                SourceBuf = file.read(file_size);
                                                Ymodem::PrepareDataPacket(SourceBuf, data, pktNo, file_size);
                                                state = END_CHECK;
                                            }
                                            port->write(data);
                                            port->flush();      //刷新下缓冲区，使串口立即发送
                                            port->waitForBytesWritten(1);
                                            qDebug() << "FIRST_SEND" << pktNo;
                                            pktNo++;
                                        }
                                    }
                                    break;
                                case SEND:
                                    if (rec.at(0) == ACK)
                                    {
                                        data.clear();
                                        if (file_size >= PACKET_1K_SIZE)    //大于等于1024
                                        {
                                            SourceBuf = file.read(PACKET_1K_SIZE);
                                            Ymodem::PrepareDataPacket(SourceBuf, data, pktNo, PACKET_1K_SIZE);
                                            file_size -= PACKET_1K_SIZE;
                                            if (file_size == 0) //等于1024
                                            {
                                                state = END_CHECK;
                                            }
                                        }
                                        else    //小于1024,一包可以发完
                                        {
                                            SourceBuf = file.read(file_size);
                                            Ymodem::PrepareDataPacket(SourceBuf, data, pktNo, file_size);
                                            state = END_CHECK;
                                        }
                                        port->write(data);
                                        port->flush();      //刷新下缓冲区，使串口立即发送
                                        port->waitForBytesWritten(1);
                                        qDebug() << "SEND" << pktNo;
                                        pktNo++;
                                    }
                                    break;
                                case END_CHECK:
                                    if (rec.at(0) == ACK && end_cnt == 0)
                                    {
                                        data.clear();
                                        data.append(EOT);
                                        port->write(data);
                                        port->flush();      //刷新下缓冲区，使串口立即发送
                                        port->waitForBytesWritten(1);
                                        qDebug() << "END_CHECK" << end_cnt;
                                        end_cnt = 1;
                                    }
                                    else if (rec.at(0) == NAK && end_cnt == 1)
                                    {
                                        data.clear();
                                        data.append(EOT);
                                        port->write(data);
                                        port->flush();      //刷新下缓冲区，使串口立即发送
                                        port->waitForBytesWritten(1);
                                        qDebug() << "END_CHECK" << end_cnt;
                                        state = END;
                                    }
                                    break;
                                case END:
                                    if (rec.size() >= 2)
                                    {
                                        if (rec.at(0) == ACK && rec.at(1) == 'C')
                                        {
                                            data.clear();
                                            Ymodem::PrepareEndPacket(data);
                                            port->write(data);
                                            port->flush();      //刷新下缓冲区，使串口立即发送
                                            port->waitForBytesWritten(1);
                                            qDebug() << "END";
                                            session_done = true;
                                        }
                                    }
                                    break;
                                default:
                                    break;
                                }
                            }
                        }
                        else
                        {
                            time_out_cnt++;
                            if (time_out_cnt == 5)
                                break;
                            qDebug() << "time_out";
                        }
                    }
                }
                else
                {
                    qDebug() << "file open fail";
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




