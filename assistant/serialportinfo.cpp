#include "serialportinfo.h"

SerialPortInfo::SerialPortInfo(QWidget *parent) : QWidget(parent)
{
    setParent(parent);
    hide();
    this->registerEvent();
    portsAvailable.clear();
    portsUsing.clear();
    for (const auto &info : QSerialPortInfo::availablePorts())
    {
        portsAvailable << (info.portName() + ':' + info.description());
    }
}

SerialPortInfo::~SerialPortInfo() = default;

#ifdef Q_OS_WINDOWS
/**
 * @brief 注册串口插拔事件
 */
void SerialPortInfo::registerEvent()
{
    static const GUID GUID_DEVINTERFACE_LIST[] = {
        // GUID_DEVINTERFACE_USB_DEVICE
        //    {0xA5DCBF10, 0x6530, 0x11D2, { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED } },
        //    // GUID_DEVINTERFACE_DISK
        //    { 0x53f56307, 0xb6bf, 0x11d0, { 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b } },
        //    // GUID_DEVINTERFACE_HID,
        //    { 0x4D1E55B2, 0xF16F, 0x11CF, { 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } },
        //    // GUID_NDIS_LAN_CLASS
        //    { 0xad498944, 0x762f, 0x11d0, { 0x8d, 0xcb, 0x00, 0xc0, 0x4f, 0xc3, 0x35, 0x8c } },
        //    // GUID_DEVINTERFACE_COMPORT
        {0x86e0d1e0, 0x8089, 0x11d0, {0x9c, 0xe4, 0x08, 0x00, 0x3e, 0x30, 0x1f, 0x73}},
        //    // GUID_DEVINTERFACE_SERENUM_BUS_ENUMERATOR
        //    { 0x4D36E978, 0xE325, 0x11CE, { 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18 } },
        //    // GUID_DEVINTERFACE_PARALLEL
        //    { 0x97F76EF0, 0xF883, 0x11D0, { 0xAF, 0x1F, 0x00, 0x00, 0xF8, 0x00, 0x84, 0x5C } },
        //    // GUID_DEVINTERFACE_PARCLASS
        //    { 0x811FC6A5, 0xF728, 0x11D0, { 0xA5, 0x37, 0x00, 0x00, 0xF8, 0x75, 0x3E, 0xD1 } },
    };
    // 注册插拔事件
    HDEVNOTIFY hDevNotify;
    DEV_BROADCAST_DEVICEINTERFACE NotifacationFiler;
    ZeroMemory(&NotifacationFiler, sizeof(DEV_BROADCAST_DEVICEINTERFACE));
    NotifacationFiler.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    NotifacationFiler.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    for (auto i : GUID_DEVINTERFACE_LIST)
    {
        NotifacationFiler.dbcc_classguid = i;
        // GetCurrentUSBGUID();
        hDevNotify = RegisterDeviceNotification(HANDLE(this->winId()), &NotifacationFiler, DEVICE_NOTIFY_WINDOW_HANDLE);
        if (!hDevNotify)
        {
            GetLastError();
        }
    }
}

bool SerialPortInfo::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    MSG *msg = reinterpret_cast<MSG *>(message); // 第一层解算
    UINT msgType = msg->message;
    if (msgType == WM_DEVICECHANGE)
    {
        auto lParam = PDEV_BROADCAST_HDR(msg->lParam); // 第二层解算
        switch (msg->wParam)
        {
        case DBT_DEVICEARRIVAL: // 设备插入
            if (lParam->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
            {
                qDebug() << "DBT_DEVICEARRIVAL";
                portsAvailable.clear();
                for (const auto &info : QSerialPortInfo::availablePorts())
                {
                    portsAvailable << (info.portName() + ':' + info.description());
                }
                emit update(portsAvailable);
            }
            break;
        case DBT_DEVICEREMOVECOMPLETE: // 设备移除
            if (lParam->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
            {
                qDebug() << "DBT_DEVICEREMOVECOMPLETE";
                portsAvailable.clear();
                for (const auto &info : QSerialPortInfo::availablePorts())
                {
                    portsAvailable << (info.portName() + ':' + info.description());
                }
                if (!portsUsing.empty())
                {
                    for (const auto &uart : portsUsing)
                    {
                        if (!portsAvailable.contains(uart))
                        {
                            qDebug() << uart;
                            emit disconnected(uart);
                            this->unregisterUsingSerialPort(uart);
                        }
                    }
                }
                emit update(portsAvailable);
            }
            break;
        }
    }
    return false;
}
#endif

/**
 * @brief 注册要使用的串口
 * @param comport 串口名带描述符，用":"隔开
 */
void SerialPortInfo::registerUsingSerialPort(const QString &comport)
{
    portsUsing.append(comport);
}

/**
 * @brief 取消注册使用的串口
 * @param comport 串口名带描述符，用":"隔开
 * @return 是否成功取消注册
 */
bool SerialPortInfo::unregisterUsingSerialPort(const QString &comport)
{
    return portsUsing.removeOne(comport);
}

/**
 * @brief 获取可用的端口
 * @return 可用的端口的列表
 */
QStringList SerialPortInfo::availablePorts()
{
    QStringList portList;
    for (const auto &info : QSerialPortInfo::availablePorts())
    {
        portList << (info.portName() + ':' + info.description());
    }
    return portList;
}
