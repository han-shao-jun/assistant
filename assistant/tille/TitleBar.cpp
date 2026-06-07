#include "TitleBar.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScreen>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>
#endif

// ═══════════════════════════════════════════════════════════════
// 构造函数 & 析构函数
// ═══════════════════════════════════════════════════════════════

TitleBar::TitleBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(_barHeight);
    setMouseTracking(true);
    setObjectName("TitleBar");
    setStyleSheet("#TitleBar{background-color:#1E1E1E;}");
    initUI();
}

TitleBar::~TitleBar()
{
}

// ═══════════════════════════════════════════════════════════════
// 公开接口
// ═══════════════════════════════════════════════════════════════

void TitleBar::setupWindow(QWidget* targetWindow)
{
    if (!targetWindow)
        return;
    _targetWindow = targetWindow;

    // 安装事件过滤器，拦截 resize / show / close / mouse 事件
    _targetWindow->installEventFilter(this);

    // 保证窗口有原生句柄
    _targetWindow->setAttribute(Qt::WA_NativeWindow);

#ifdef Q_OS_WIN
    // ╾ 关键：Windows 下不设置 FramelessWindowHint ╾
    // 标准窗口样式 (WS_OVERLAPPEDWINDOW) 保留了 WS_THICKFRAME + WS_MAXIMIZEBOX，
    // 这是 Snap Layouts 和 WM_NCHITTEST 正常工作的前提。
    // 通过 WM_NCCALCSIZE 将客户区扩展到标题栏位置，用 TitleBar QWidget 替代系统标题栏。
    // applyWinWindowStyle() 在 Show 事件中微调样式（移除 WS_SYSMENU）。

    // 图标和标题同步
    connect(_targetWindow, &QWidget::windowIconChanged, this, [this](const QIcon& icon) {
        _iconLabel->setPixmap(icon.pixmap(18, 18));
        _iconLabel->setVisible(!icon.isNull());
    });

    if (!_targetWindow->windowIcon().isNull()) {
        _iconLabel->setPixmap(_targetWindow->windowIcon().pixmap(18, 18));
    } else {
        _iconLabel->setVisible(false);
    }

    if (!_targetWindow->windowTitle().isEmpty()) {
        _titleLabel->setText(_targetWindow->windowTitle());
    } else {
        _titleLabel->setVisible(false);
    }

    // 监听标题变化
    connect(_targetWindow, &QWidget::windowTitleChanged, this, [this](const QString& title) {
        _titleLabel->setText(title);
        _titleLabel->setVisible(!title.isEmpty());
    });
#else
    // 非 Windows 平台：直接用 FramelessWindowHint
    _targetWindow->setWindowFlags(
        Qt::Window | Qt::FramelessWindowHint |
        Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint |
        Qt::WindowCloseButtonHint);
    _targetWindow->setAttribute(Qt::WA_Hover);
#endif

    // 在 TitleBar 下方留出空间
    _targetWindow->setContentsMargins(0, _barHeight, 0, 0);

    // 同步图标和标题
    if (!_targetWindow->windowIcon().isNull()) {
        _iconLabel->setPixmap(_targetWindow->windowIcon().pixmap(18, 18));
    }
    if (!_targetWindow->windowTitle().isEmpty()) {
        _titleLabel->setText(_targetWindow->windowTitle());
    }
}

void TitleBar::setTitle(const QString& title)
{
    _titleLabel->setText(title);
    _titleLabel->setVisible(!title.isEmpty());
}

QString TitleBar::title() const
{
    return _titleLabel->text();
}

void TitleBar::setWindowIcon(const QIcon& icon)
{
    _iconLabel->setPixmap(icon.pixmap(18, 18));
    _iconLabel->setVisible(true);
}

void TitleBar::addLeftWidget(QWidget* widget)
{
    widget->setParent(this);
    _leftAreaLayout->addWidget(widget);
    _leftArea->setVisible(true);
}

void TitleBar::addMiddleWidget(QWidget* widget)
{
    widget->setParent(this);
    _middleAreaLayout->addWidget(widget);
    _middleArea->setVisible(true);
}

void TitleBar::addRightWidget(QWidget* widget)
{
    widget->setParent(this);
    _rightAreaLayout->addWidget(widget);
    _rightArea->setVisible(true);
}

void TitleBar::setMinimizeButtonVisible(bool visible)
{
    _minButton->setVisible(visible);
}

void TitleBar::setMaximizeButtonVisible(bool visible)
{
    _maxButton->setVisible(visible);
}

void TitleBar::setCloseButtonVisible(bool visible)
{
    _closeButton->setVisible(visible);
}

void TitleBar::setFixedWindowSize(bool fixed)
{
    _fixedSize = fixed;
    _maxButton->setVisible(!fixed);
#ifdef Q_OS_WIN
    if (_targetWindow) {
        HWND hwnd = reinterpret_cast<HWND>(_targetWindow->winId());
        DWORD style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
        if (fixed) {
            style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
        } else {
            style |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
        }
        ::SetWindowLongPtrW(hwnd, GWL_STYLE, style);
        // 通知系统窗口样式已变更
        ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                       SWP_NOZORDER | SWP_NOOWNERZORDER |
                       SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
    }
#endif
}

void TitleBar::setBarHeight(int height)
{
    _barHeight = height;
    setFixedHeight(_barHeight);

    // 更新按钮尺寸与图标大小，保持与 initUI 中的计算一致
    int btnW = _barHeight + 4;
    int iconSz = qBound(14, _barHeight - 12, 28);

    if (_minButton)    _minButton->setFixedSize(btnW, _barHeight);
    if (_maxButton)    _maxButton->setFixedSize(btnW, _barHeight);
    if (_closeButton)  _closeButton->setFixedSize(btnW, _barHeight);

    QSize isz(iconSz, iconSz);
    if (_minButton)    _minButton->setIconSize(isz);
    if (_maxButton)    _maxButton->setIconSize(isz);
    if (_closeButton)  _closeButton->setIconSize(isz);

    if (_targetWindow)
        _targetWindow->setContentsMargins(0, _barHeight, 0, 0);
}

int TitleBar::barHeight() const
{
    return _barHeight;
}

// ═══════════════════════════════════════════════════════════════
// 内部布局
// ═══════════════════════════════════════════════════════════════

void TitleBar::initUI()
{
    _mainLayout = new QHBoxLayout(this);
    _mainLayout->setContentsMargins(0, 0, 0, 0);
    _mainLayout->setSpacing(0);

    // ---- 窗口图标 ----
    _iconLabel = new QLabel(this);
    _iconLabel->setFixedSize(24, 24);
    _iconLabel->setAlignment(Qt::AlignCenter);

    // ---- 窗口标题 ----
    _titleLabel = new QLabel(this);
    _titleLabel->setStyleSheet("color:#e0e0e0; font-size:12px; padding-left:4px;");

    // ---- 三个自定义区域 ----
    _leftArea   = new QWidget(this);
    _middleArea = new QWidget(this);
    _rightArea  = new QWidget(this);
    _leftArea->setVisible(false);
    _middleArea->setVisible(false);
    _rightArea->setVisible(false);

    _leftAreaLayout   = new QHBoxLayout(_leftArea);
    _leftAreaLayout->setContentsMargins(0, 0, 0, 0);
    _leftAreaLayout->setSpacing(4);

    _middleAreaLayout = new QHBoxLayout(_middleArea);
    _middleAreaLayout->setContentsMargins(0, 0, 0, 0);
    _middleAreaLayout->setSpacing(4);

    _rightAreaLayout  = new QHBoxLayout(_rightArea);
    _rightAreaLayout->setContentsMargins(0, 0, 0, 0);
    _rightAreaLayout->setSpacing(4);

    // ---- 窗口控制按钮（沿用原 assistant 项目图标，尺寸自适应） ----
    int btnW = _barHeight + 4;                     // 宽度：36(32px) / 52(48px)
    int iconSz = qBound(14, _barHeight - 12, 28);  // 图标：24(36px) / 36→28(48px)

    auto makeIconBtn = [this, btnW, iconSz](const QIcon& icon, const QString& style) -> QToolButton* {
        auto* btn = new QToolButton(this);
        btn->setIcon(icon);
        btn->setIconSize(QSize(iconSz, iconSz));
        btn->setFixedSize(btnW, _barHeight);
        btn->setAutoRaise(true);
        btn->setStyleSheet(style);
        return btn;
    };

    _minButton = makeIconBtn(QIcon(":/image/min.svg"), R"(
        QToolButton { border: none; }
        QToolButton:hover { background-color: #3a3a4a; }
    )");
    _maxButton = makeIconBtn(QIcon(":/image/max.svg"), R"(
        QToolButton { border: none; }
        QToolButton:hover { background-color: #3a3a4a; }
        QToolButton[hovered="true"] { background-color: #3a3a4a; }
    )");
    _closeButton = makeIconBtn(QIcon(":/image/close.svg"), R"(
        QToolButton { border: none; }
        QToolButton:hover { background-color: #e81123; }
    )");

    // 按钮点击信号
    connect(_minButton, &QToolButton::clicked, this, [this]() {
        if (_targetWindow) _targetWindow->showMinimized();
        emit minimizeClicked();
    });
    connect(_maxButton, &QToolButton::clicked, this, [this]() {
        if (!_targetWindow) return;
        if (_targetWindow->isMaximized())
            _targetWindow->showNormal();
        else
            _targetWindow->showMaximized();
        emit maximizeClicked();
    });
    connect(_closeButton, &QToolButton::clicked, this, [this]() {
        if (_targetWindow) _targetWindow->close();
        emit closeClicked();
    });

    // ---- 组装布局 ----
    // 左侧：图标 | 标题 | 自定义左侧区域
    QHBoxLayout* leftLayout = new QHBoxLayout();
    leftLayout->setContentsMargins(8, 0, 0, 0);
    leftLayout->setSpacing(4);
    leftLayout->addWidget(_iconLabel);
    leftLayout->addWidget(_titleLabel);
    leftLayout->addWidget(_leftArea);
    leftLayout->addStretch();

    // 右侧：自定义右侧区域 | 最小化 | 最大化 | 关闭
    QHBoxLayout* rightLayout = new QHBoxLayout();
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    rightLayout->addStretch();
    rightLayout->addWidget(_rightArea);
    rightLayout->addWidget(_minButton);
    rightLayout->addWidget(_maxButton);
    rightLayout->addWidget(_closeButton);

    _mainLayout->addLayout(leftLayout);
    _mainLayout->addWidget(_middleArea, 1);  // 中间区域占满剩余空间
    _mainLayout->addLayout(rightLayout);
}

// ═══════════════════════════════════════════════════════════════
// 事件过滤器 (跨平台入口)
// ═══════════════════════════════════════════════════════════════

bool TitleBar::eventFilter(QObject* obj, QEvent* event)
{
    if (obj != _targetWindow)
        return QWidget::eventFilter(obj, event);

    switch (event->type())
    {
    case QEvent::Resize:
        // TitleBar 宽度跟随窗口
        if (_targetWindow)
            resize(_targetWindow->width(), _barHeight);
        break;

#ifdef Q_OS_WIN
    case QEvent::Show:
        applyWinWindowStyle();
        break;

    case QEvent::WindowStateChange: {
        bool maximized = _targetWindow->isMaximized();
        updateMaxButtonIcon(maximized);
        updateWinWindowMargins(maximized);
        // 确保最大化/还原后图标尺寸不变
        int iconSz = qBound(14, _barHeight - 12, 28);
        QSize isz(iconSz, iconSz);
        if (_minButton)   _minButton->setIconSize(isz);
        if (_maxButton)   _maxButton->setIconSize(isz);
        if (_closeButton) _closeButton->setIconSize(isz);
        break;
    }
#endif

#ifndef Q_OS_WIN
    // 非 Windows 平台：用 Qt 的方式处理拖拽和调整大小
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonRelease:
    case QEvent::HoverMove:
        handleMouseEvents(obj, event);
        break;
#endif

    default:
        break;
    }
    return QWidget::eventFilter(obj, event);
}

// ═══════════════════════════════════════════════════════════════
// 绘制
// ═══════════════════════════════════════════════════════════════

void TitleBar::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 底部细线
    painter.setPen(QColor(0x30, 0x30, 0x40));
    painter.drawLine(0, height() - 1, width(), height() - 1);
}

// ═══════════════════════════════════════════════════════════════
// Windows 原生消息处理
// ═══════════════════════════════════════════════════════════════

#ifdef Q_OS_WIN

void TitleBar::applyWinWindowStyle()
{
    // 标准 Qt 窗口已有 WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX
    // 只需移除 WS_SYSMENU（禁用默认系统菜单，我们用自己的）
    if (!_targetWindow)
        return;

    HWND hwnd = reinterpret_cast<HWND>(_targetWindow->winId());
    if (!hwnd)
        return;

    DWORD style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
    style &= ~WS_SYSMENU;  // 移除系统菜单，保留 THICKFRAME + MAXIMIZEBOX
    ::SetWindowLongPtrW(hwnd, GWL_STYLE, style);

    // 通过 DwmExtendFrameIntoClientArea 把 DWM 非客户区渲染扩展到客户区
    // 设为全0表示由我们通过 WM_NCCALCSIZE 自行管理
    MARGINS margins = {0, 0, 0, 0};
    ::DwmExtendFrameIntoClientArea(hwnd, &margins);

    // 通知系统刷新窗口样式
    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                   SWP_NOZORDER | SWP_NOOWNERZORDER |
                   SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
}

void TitleBar::updateWinWindowMargins(bool maximized)
{
    // 最大化时不需要边框留白（边框被裁剪了），正常状态保留边框
    if (maximized) {
        _resizeMargin = 0;
    } else {
        _resizeMargin = 8;
    }
    // 强制重绘以更新 hit-test 区域
    if (_targetWindow) {
        HWND hwnd = reinterpret_cast<HWND>(_targetWindow->winId());
        ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                       SWP_NOZORDER | SWP_NOOWNERZORDER |
                       SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
    }
}

// 核心消息1: WM_NCCALCSIZE — 将客户区扩展到窗口顶部
bool TitleBar::handleWinNcCalcSize(void* message, qintptr* result)
{
    const auto msg = static_cast<const MSG*>(message);
    HWND hwnd = msg->hwnd;
    WPARAM wParam = msg->wParam;
    LPARAM lParam = msg->lParam;

    if (wParam == FALSE)
        return false;  // 不需要处理（wParam=FALSE 表示无需计算，只通知）

    NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
    RECT* clientRect = &params->rgrc[0];

    // ═══════════════════════════════════════════════════════
    // 关键：先保存原始顶部位置，然后让系统计算非客户区
    // ═══════════════════════════════════════════════════════
    const LONG originTop = clientRect->top;

    // 调用 DefWindowProcW 让系统计算标准非客户区
    // 这一步对 Snap Layouts 至关重要——系统内部会记录正确的几何信息
    ::DefWindowProcW(hwnd, WM_NCCALCSIZE, wParam, lParam);

    // ═══════════════════════════════════════════════════════
    // 关键：只修改 clientRect->top，左右下保持 DefWindowProcW 计算的值
    // 系统已经正确计算了边框厚度和最大化裁剪，我们只把顶部扩展入标题栏区域
    // ═══════════════════════════════════════════════════════
    if (::IsZoomed(hwnd)) {
        // 最大化时 DefWindowProcW 已将边框裁剪
        // 只需恢复顶部到屏幕边缘（originTop 就是最大化窗口的原始顶部）
        clientRect->top = originTop;
    } else {
        // 正常状态：恢复 clientRect->top 到窗口原始顶部
        // DefWindowProcW 之后 clientRect->top 已被下移（留出标题栏空间）
        clientRect->top = originTop;
    }

    *result = WVR_REDRAW;
    return true;
}

// 核心消息2: WM_NCHITTEST — 告诉系统鼠标在窗口的哪个部位
bool TitleBar::handleWinNcHitTest(void* message, qintptr* result)
{
    const auto msg = static_cast<const MSG*>(message);
    HWND hwnd = msg->hwnd;
    LPARAM lParam = msg->lParam;

    // 先检查最大化按钮——返回 HTZOOM 让系统处理 snap 菜单
    // 手动管理 hover 状态，因为 HTZOOM 导致 Qt 收不到鼠标事件
    if (_maxButton->isVisible() && containsCursor(_maxButton)) {
        if (!_hoverMaxButton) {
            _hoverMaxButton = true;
            _maxButton->setProperty("hovered", true);
            _maxButton->style()->unpolish(_maxButton);
            _maxButton->style()->polish(_maxButton);
        }
        *result = HTZOOM;
        return true;
    }
    if (_hoverMaxButton) {
        _hoverMaxButton = false;
        _maxButton->setProperty("hovered", false);
        _maxButton->style()->unpolish(_maxButton);
        _maxButton->style()->polish(_maxButton);
    }

    // 获取鼠标在窗口客户区坐标
    POINT nativePos{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ::ScreenToClient(hwnd, &nativePos);

    RECT clientRect;
    ::GetClientRect(hwnd, &clientRect);
    int clientWidth  = clientRect.right  - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;

    // 判断鼠标是否在窗口边缘（用于调整大小）
    bool left   = nativePos.x < _resizeMargin;
    bool right  = nativePos.x > clientWidth  - _resizeMargin;
    bool top    = nativePos.y < _resizeMargin;
    bool bottom = nativePos.y > clientHeight - _resizeMargin;

    *result = HTNOWHERE;

    if (!_fixedSize && !_targetWindow->isMaximized() && !_targetWindow->isFullScreen()) {
        if      (left && top)    *result = HTTOPLEFT;
        else if (left && bottom) *result = HTBOTTOMLEFT;
        else if (right && top)   *result = HTTOPRIGHT;
        else if (right && bottom)*result = HTBOTTOMRIGHT;
        else if (left)           *result = HTLEFT;
        else if (right)          *result = HTRIGHT;
        else if (top)            *result = HTTOP;
        else if (bottom)         *result = HTBOTTOM;
    }

    if (*result != HTNOWHERE)
        return true;

    // 如果鼠标在 TitleBar 上（且不在子控件上）→ 返回 HTCAPTION（可拖拽移动窗口）
    if (containsCursor(this) && !_targetWindow->isFullScreen()) {
        *result = HTCAPTION;
        return true;
    }

    // 其余区域 → 客户区
    *result = HTCLIENT;
    return true;
}

// 辅助消息: WM_SIZE — 跟踪窗口最大化/还原状态
bool TitleBar::handleWinSize(void* message)
{
    const auto msg = static_cast<const MSG*>(message);
    WPARAM wParam = msg->wParam;
    if (wParam == SIZE_RESTORED)
        updateMaxButtonIcon(false);
    else if (wParam == SIZE_MAXIMIZED)
        updateMaxButtonIcon(true);
    return false;  // 不拦截，让默认处理继续
}

// 辅助消息: WM_GETMINMAXINFO — 设置最小窗口尺寸
bool TitleBar::handleWinGetMinMaxInfo(void* message, qintptr* result)
{
    Q_UNUSED(result);
    const auto msg = static_cast<const MSG*>(message);
    LPARAM lParam = msg->lParam;
    MINMAXINFO* minmax = reinterpret_cast<MINMAXINFO*>(lParam);

    // 计算标题栏最小宽度（基于可见控件）
    int minWidth = 0;
    if (_iconLabel->isVisible())  minWidth += 24 + 4;
    if (_titleLabel->isVisible()) minWidth += _titleLabel->minimumWidth() + 4;
    if (_minButton->isVisible())  minWidth += 46;
    if (_maxButton->isVisible())  minWidth += 46;
    if (_closeButton->isVisible())minWidth += 46;
    minWidth = qMax(minWidth, 200);  // 不小于200px

    minmax->ptMinTrackSize.x = static_cast<LONG>(minWidth);
    minmax->ptMinTrackSize.y = static_cast<LONG>(_barHeight * 2);

    return true;
}

// 辅助消息: WM_NCLBUTTONDOWN — 阻止 Windows 接管最大化按钮的点击
bool TitleBar::handleWinNcLButtonDown(void* /*message*/, qintptr* /*result*/)
{
    if (_maxButton->isVisible() && containsCursor(_maxButton)) {
        return true;  // 拦截，不让 Windows 处理
    }
    return false;
}

// 辅助消息: WM_NCLBUTTONUP — 手动触发最大化/还原
bool TitleBar::handleWinNcLButtonUp(void* message, qintptr* /*result*/)
{
    if (_maxButton->isVisible() && containsCursor(_maxButton) && _targetWindow) {
        // 使用 PostMessage 异步发送最大化命令，避免在消息处理中的重入问题
        const auto msg = static_cast<const MSG*>(message);
        HWND hwnd = msg->hwnd;
        WPARAM cmd = _targetWindow->isMaximized() ? SC_RESTORE : SC_MAXIMIZE;
        ::PostMessageW(hwnd, WM_SYSCOMMAND, cmd, 0);
        return true;
    }
    return false;
}

#endif // Q_OS_WIN

// ═══════════════════════════════════════════════════════════════
// 非 Windows 平台：Qt 方式处理拖拽和调整大小
// ═══════════════════════════════════════════════════════════════

#ifndef Q_OS_WIN
void TitleBar::handleMouseEvents(QObject* obj, QEvent* event)
{
    Q_UNUSED(obj);
    if (!_targetWindow || !_targetWindow->windowHandle())
        return;

    switch (event->type())
    {
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() != Qt::LeftButton)
            break;
        if (_edges != 0) {
            // 边缘 → 调整大小
            _targetWindow->windowHandle()->startSystemResize(
                static_cast<Qt::Edges>(_edges));
        } else if (containsCursor(this)) {
            // 标题栏 → 拖拽移动
            _targetWindow->windowHandle()->startSystemMove();
        }
        break;
    }
    case QEvent::MouseButtonDblClick: {
        if (containsCursor(this) && !_fixedSize) {
            if (_targetWindow->isMaximized())
                _targetWindow->showNormal();
            else
                _targetWindow->showMaximized();
        }
        break;
    }
    case QEvent::HoverMove: {
        if (_targetWindow->isMaximized() || _targetWindow->isFullScreen() || _fixedSize) {
            _edges = 0;
            break;
        }
        auto* me = static_cast<QHoverEvent*>(event);
        QPoint p = me->pos();
        _edges = 0;
        if (p.x() < _resizeMargin)           _edges |= Qt::LeftEdge;
        if (p.x() > _targetWindow->width() - _resizeMargin)  _edges |= Qt::RightEdge;
        if (p.y() < _resizeMargin)           _edges |= Qt::TopEdge;
        if (p.y() > _targetWindow->height() - _resizeMargin) _edges |= Qt::BottomEdge;

        // 设置光标
        switch (_edges) {
        case Qt::LeftEdge:
        case Qt::RightEdge:
            _targetWindow->setCursor(Qt::SizeHorCursor); break;
        case Qt::TopEdge:
        case Qt::BottomEdge:
            _targetWindow->setCursor(Qt::SizeVerCursor); break;
        case Qt::LeftEdge | Qt::TopEdge:
        case Qt::RightEdge | Qt::BottomEdge:
            _targetWindow->setCursor(Qt::SizeFDiagCursor); break;
        case Qt::RightEdge | Qt::TopEdge:
        case Qt::LeftEdge | Qt::BottomEdge:
            _targetWindow->setCursor(Qt::SizeBDiagCursor); break;
        default:
            _targetWindow->setCursor(Qt::ArrowCursor); break;
        }
        break;
    }
    default:
        break;
    }
}
#endif

// ═══════════════════════════════════════════════════════════════
// 辅助函数
// ═══════════════════════════════════════════════════════════════

bool TitleBar::containsCursor(QWidget* widget) const
{
    if (!widget || !widget->isVisible())
        return false;

    // TitleBar 自身：排除子控件区域
    if (widget == this) {
        // 1. 排除窗口控制按钮（最小化/最大化/关闭）
        for (QWidget* child : clientWidgets()) {
            if (containsCursor(child))
                return false;
        }
        // 2. 排除自定义区域中实际可交互的子控件
        //    只排除按钮、输入框等（focusPolicy != NoFocus），
        //    标签、空白区域等仍可拖拽
        QList<QWidget*> areas = {_leftArea, _middleArea, _rightArea};
        for (QWidget* area : areas) {
            if (area->isVisible()) {
                const auto children = area->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
                for (QWidget* child : children) {
                    if (child->isVisible()
                        && child->focusPolicy() != Qt::NoFocus   // 可交互控件
                        && containsCursor(child))
                        return false;
                }
            }
        }
    }

    QPoint globalPos = QCursor::pos();
    QRect widgetRect(widget->mapToGlobal(QPoint(0, 0)), widget->size());
    return widgetRect.contains(globalPos);
}

QList<QWidget*> TitleBar::clientWidgets() const
{
    QList<QWidget*> list;
    list << _minButton << _maxButton << _closeButton;
    return list;
}

void TitleBar::updateMaxButtonIcon(bool isMaximized)
{
    _maxButton->setIcon(isMaximized ? QIcon(":/image/restore.svg") : QIcon(":/image/max.svg"));
}
