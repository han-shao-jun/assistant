#include "mainwin.h"
#include "widget.h"
#include <QtNetwork/QNetworkProxyFactory>
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    QApplication app(argc, argv);

    MainWin win;
    win.show();

    // 获取系统的默认locale
    QLocale systemLocale = QLocale::system();

    // 获取操作系统使用的语言
    QString language = QLocale::languageToString(systemLocale.language());

    qDebug() << "Operating System Language:" << language;
    QNetworkProxyFactory::setUseSystemConfiguration(false);

    QList<QNetworkProxy> listOfProxies = QNetworkProxyFactory::systemProxyForQuery();
    qDebug() << "system proxy" << listOfProxies;
    return QApplication::exec();
}
