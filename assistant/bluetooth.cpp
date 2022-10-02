#include "bluetooth.h"

Bluetooth::Bluetooth(QObject *parent): QThread(parent)
{
    this->setParent(parent);

}
