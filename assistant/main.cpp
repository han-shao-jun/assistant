#include "mainwin.h"
#include "widget.h"
#include <QtNetwork/QNetworkProxyFactory>
#include <QtWidgets/QApplication>
#include <QtGui/QGuiApplication>

int main(int argc, char *argv[])
{
    // ═══ 高 DPI 适配（必须在 QApplication 构造之前设置） ═══
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt 5：手动启用高 DPI 缩放
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    // Qt 6：PassThrough 避免非整数缩放比的取整模糊
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    // ═══════════════════════════════════════════════════════════

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
