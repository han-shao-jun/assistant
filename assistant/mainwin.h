#ifndef MAINWIN_H
#define MAINWIN_H

#include "TitleBar.h"
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QToolButton>

class Widget;

class MainWin : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWin(QWidget *parent = nullptr);
    ~MainWin() override;

    /// 获取标题栏对象，用于添加自定义控件
    TitleBar *titleBar() const;

    /// 设置窗口标题（覆盖 QWidget::setWindowTitle）
    void setWindowTitle(const QString &title);

    /// 设置窗口图标
    void setWindowIcon(const QIcon &icon);

protected:
#ifdef Q_OS_WIN
    /// Windows 原生消息入口 — 转发给 TitleBar 处理
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif
    void resizeEvent(QResizeEvent *event) override;

private:
    void initUI();

    TitleBar *_titleBar = nullptr;
    QToolButton *_pinBtn = nullptr;
    Widget *_contentWidget = nullptr;
    bool _isTop = false;
};

#endif // MAINWIN_H
