#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <QThread>
#include <QtBluetooth>


class Bluetooth : public QThread
{
    Q_OBJECT
public:
    explicit Bluetooth(QObject *parent  = nullptr);
};

#endif // BLUETOOTH_H
