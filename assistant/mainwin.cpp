#include "mainwin.h"
#include "TitleBar.h"
#include "widget.h"

#include <QIcon>
#include <QResizeEvent>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

MainWin::MainWin(QWidget* parent)
    : QMainWindow(parent)
{
    initUI();
}

MainWin::~MainWin()
{
}

void MainWin::initUI()
{
    // 基础窗口设置
    resize(960, 650);
    setMinimumSize(300, 200);

    // ---- 创建标题栏（参照 logic/MainWindow.cpp） ----
    _titleBar = new TitleBar(this);

    // 设为原 assistant 标题栏高度 36px
    _titleBar->setBarHeight(36);

    // 设置图标和标题（沿用原 assistant 项目的品牌标识）
    this->setWindowIcon(QIcon(":/image/logo.ico"));
    this->setWindowTitle(QString::fromUtf8("多功能调试助手"));

    _titleBar->setupWindow(this);

    // 确保 TitleBar 可见并位于最上层
    _titleBar->show();
    _titleBar->raise();
    _titleBar->resize(width(), _titleBar->barHeight());

    // 置顶按钮 — 作为标题栏的自定义右侧控件
    _pinBtn = new QToolButton(this);
    _pinBtn->setToolTip(tr("置顶窗口"));
    _pinBtn->setCursor(Qt::PointingHandCursor);
    _pinBtn->setFixedSize(32, 32);
    _pinBtn->setIcon(QIcon(":/image/ding1.png"));
    _pinBtn->setStyleSheet(R"(
        QToolButton { border: none; background: transparent; }
        QToolButton:hover { background-color: rgba(51,127,209,230); border-radius: 3px; }
    )");
    connect(_pinBtn, &QToolButton::clicked, this, [this]() {
        _isTop = !_isTop;
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(this->winId());
        if (_isTop) {
            ::SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                           SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            _pinBtn->setIcon(QIcon(":/image/ding2.png"));
        } else {
            ::SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                           SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            _pinBtn->setIcon(QIcon(":/image/ding1.png"));
        }
#endif
    });
    _titleBar->addRightWidget(_pinBtn);

    // ---- 创建中心内容区域（参照 logic/MainWindow.cpp） ----
    QWidget* centralWidget = new QWidget(this);
    centralWidget->setObjectName("CentralWidget");
    centralWidget->setStyleSheet("#CentralWidget{background-color:#1E1E1E;}");
    setCentralWidget(centralWidget);

    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    _contentWidget = new Widget(this);
    _contentWidget->loadStyleSheet(":/style/center.css");
    layout->addWidget(_contentWidget);
}

TitleBar* MainWin::titleBar() const
{
    return _titleBar;
}

void MainWin::setWindowTitle(const QString& title)
{
    QMainWindow::setWindowTitle(title);
    _titleBar->setTitle(title);
}

void MainWin::setWindowIcon(const QIcon& icon)
{
    QMainWindow::setWindowIcon(icon);
    _titleBar->setWindowIcon(icon);
}

void MainWin::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    // TitleBar 宽度跟随窗口
    _titleBar->resize(event->size().width(), _titleBar->barHeight());
}

#ifdef Q_OS_WIN
bool MainWin::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    // 只处理 Windows 消息
    if (eventType != "windows_generic_MSG" || !message)
        return QMainWindow::nativeEvent(eventType, message, result);

    const auto msg = static_cast<const MSG*>(message);
    const HWND hwnd = msg->hwnd;
    if (!hwnd)
        return false;

    switch (msg->message)
    {
    // ═══════════════════════════════════════════════════
    // 核心消息：客户区扩展 —— 把标题栏区域变成客户区
    // ═══════════════════════════════════════════════════
    case WM_NCCALCSIZE:
        return _titleBar->handleWinNcCalcSize(message, result);

    // ═══════════════════════════════════════════════════
    // 核心消息：命中测试 —— 告诉系统哪里可以拖拽/调整大小
    // ═══════════════════════════════════════════════════
    case WM_NCHITTEST:
        return _titleBar->handleWinNcHitTest(message, result);

    // ═══════════════════════════════════════════════════
    // 辅助消息：窗口大小变化（跟踪最大化/还原状态）
    // ═══════════════════════════════════════════════════
    case WM_SIZE:
        _titleBar->handleWinSize(message);
        break;

    // ═══════════════════════════════════════════════════
    // 辅助消息：最小/最大尺寸约束
    // ═══════════════════════════════════════════════════
    case WM_GETMINMAXINFO:
        _titleBar->handleWinGetMinMaxInfo(message, result);
        return true;

    // ═══════════════════════════════════════════════════
    // 最大化按钮点击拦截 —— WM_NCHITTEST 返回 HTZOOM 时
    // Windows 接管了按钮点击，需要手动处理
    // ═══════════════════════════════════════════════════
    case WM_NCLBUTTONDOWN:
        if (_titleBar->handleWinNcLButtonDown(message, result))
            return true;
        break;

    case WM_NCLBUTTONUP:
        if (_titleBar->handleWinNcLButtonUp(message, result))
            return true;
        break;

    case WM_NCLBUTTONDBLCLK:
        // 双击标题栏区域：让 Windows 默认处理（最大化/还原）
        if (_titleBar->handleWinNcLButtonDown(message, result))
            return true;
        break;

    // ═══════════════════════════════════════════════════
    // 以下消息保持系统默认处理（确保 Snap 和 DWM 正常工作）
    // ═══════════════════════════════════════════════════
    case WM_NCPAINT:
        // DWM 开启时不绘制非客户区
        if (::DwmIsCompositionEnabled(nullptr) == S_OK) {
            *result = FALSE;
            return true;
        }
        break;

    case WM_NCACTIVATE:
        // 让 DWM 处理窗口激活/失活的标题栏绘制
        if (::DwmIsCompositionEnabled(nullptr) == S_OK) {
            *result = ::DefWindowProcW(hwnd, WM_NCACTIVATE, msg->wParam, -1);
        } else {
            *result = TRUE;
        }
        return true;

    case WM_SHOWWINDOW:
        // 窗口首次显示时刷新窗口样式
        if (msg->wParam == TRUE) {
            ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
        }
        break;
    }

    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif
