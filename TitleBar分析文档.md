# 「多功能调试助手」代码架构分析

## 1. 项目概览

基于 **Qt 6.7.3** 的嵌入式开发调试工具，使用 **C++17** 标准，**MSVC 2022** 静态编译。

### 功能模块

| 模块 | 页面 | 说明 |
|------|------|------|
| 通信 | `comPage` | 串口 / TCP Server / TCP Client / UDP 多协议通信 |
| PID | `pidPage` | PID 参数调参 + 4 通道实时曲线绘制 |
| 示波器 | `oscPage` | OSC 协议网络示波器，双通道波形显示 |
| 下载 | `dowPage` | STM32 ISP / IAP 固件烧录 |

### 文件结构

```
assistant/
├── CMakeLists.txt              # CMake 构建配置 (Qt6, dwmapi 链接)
├── main.cpp                    # 入口: 创建 MainWin 并启动事件循环
├── mainwin.h / mainwin.cpp     # 顶层窗口: 消息路由, TitleBar + Widget 组装
├── widget.h / widget.cpp       # 中心内容区: 4 个功能页面的全部业务逻辑
├── widget.ui                   # Qt Designer UI 文件 (~3562 行)
├── tille/
│   ├── TitleBar.h              # 自定义标题栏声明
│   └── TitleBar.cpp            # 自定义标题栏实现 (核心)
├── com/                        # 串口/网络通信线程
├── pid/                        # PID 通信线程
├── osc/                        # OSC 网络通信线程
├── dowload/                    # 固件下载 (ISP/IAP, YModem协议)
├── net/                        # 网络工具函数
├── serialport_info.h/cpp       # 串口热插拔监听
├── qcustomplot/                # QCustomPlot 图表库
└── resources/
    ├── image/                  # 图标 (close/min/max/restore/ding等)
    └── style/center.css        # 全局样式表
```

---

## 2. 窗口层级架构

```
main.cpp
  └── MainWin (QMainWindow)                    ← 顶层窗口, 拦截 Windows 原生消息
        ├── TitleBar                          ← 自定义标题栏 (替代系统标题栏)
        │     ├── leftLayout                  ← 图标 + 标题 + 自定义左侧区域
        │     ├── _middleArea                 ← 中间弹性区域 (stretch=1)
        │     └── rightLayout                 ← 自定义右侧区域 + 窗口按钮
        ├── QToolButton (_pinBtn)             ← 置顶按钮 (添加到 TitleBar 右侧)
        └── QWidget (centralWidget)           ← 中心内容容器
              └── Widget (_contentWidget)     ← QStackedWidget 承载4个页面
                    ├── comPage   (通信页面)
                    ├── pidPage   (PID调参页面)
                    ├── oscPage   (示波器页面)
                    └── dowPage   (下载页面)
```

---

## 3. 自定义标题栏 `TitleBar` 深度解析

`TitleBar` 是整个项目中最精妙的部分。

### 3.1 设计思想

**核心策略（来自源码注释）**：

> 仿照 ElaWidgetTools 的 ElaAppBar。保留 `WS_THICKFRAME | WS_MAXIMIZEBOX`（Snap Layouts 依赖这两个样式），通过 `WM_NCCALCSIZE` 把 `clientRect->top` 推到窗口顶部，让"原标题栏区域"变成客户区，然后在这里放自己的 TitleBar QWidget。

这套策略的精妙之处在于：**不是简单地去掉标题栏，而是欺骗 Windows** —— 保留原生窗口框架的所有样式位，让 DWM 和 Snap Layouts 正常工作，但通过消息拦截把系统标题栏"覆盖"掉。

### 3.2 设计决策：不设 `FramelessWindowHint`

```cpp
// TitleBar.cpp:63 — setupWindow() 中
// ╾ 关键：Windows 下不设置 FramelessWindowHint ╾
// 标准窗口样式 (WS_OVERLAPPEDWINDOW) 保留了 WS_THICKFRAME + WS_MAXIMIZEBOX，
// 这是 Snap Layouts 和 WM_NCHITTEST 正常工作的前提。
```

大多数 Qt 自定义标题栏教程会告诉你用 `Qt::FramelessWindowHint`，但这里反其道而行——刻意保留标准窗口样式。

**原因**：Win10/11 的 **Snap Layouts**（窗口拖到屏幕边缘自动分屏）依赖 `WS_THICKFRAME`，去掉它弹窗布局就废了。

### 3.3 类接口总览

```cpp
class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget* parent = nullptr);

    // ── 核心 ──
    void setupWindow(QWidget* targetWindow);      // 安装到目标窗口, 拦截原生消息

    // ── 标题 & 图标 ──
    void setTitle(const QString& title);
    QString title() const;
    void setWindowIcon(const QIcon& icon);

    // ── 自定义控件区域 ──
    void addLeftWidget(QWidget* widget);           // 添加到左侧（标题右侧）
    void addMiddleWidget(QWidget* widget);         // 添加到中间（拉伸区域）
    void addRightWidget(QWidget* widget);          // 添加到右侧（窗口按钮左侧）

    // ── 窗口按钮显隐 ──
    void setMinimizeButtonVisible(bool visible);
    void setMaximizeButtonVisible(bool visible);
    void setCloseButtonVisible(bool visible);

    // ── 尺寸控制 ──
    void setFixedWindowSize(bool fixed);           // 固定窗口大小（禁用 resize 和最大化）
    void setBarHeight(int height);                 // 标题栏高度
    int barHeight() const;

    // ── Windows 原生消息处理（公开, 供 MainWin::nativeEvent 调用）─
#ifdef Q_OS_WIN
    bool handleWinNcCalcSize(void* msg, qintptr* result);
    bool handleWinNcHitTest(void* msg, qintptr* result);
    bool handleWinSize(void* msg);
    bool handleWinGetMinMaxInfo(void* msg, qintptr* result);
    bool handleWinNcLButtonDown(void* msg, qintptr* result);
    bool handleWinNcLButtonUp(void* msg, qintptr* result);
    void applyWinWindowStyle();
    void updateWinWindowMargins(bool maximized);
#endif

signals:
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
};
```

### 3.4 内部布局结构

```
TitleBar (QWidget, 36px 默认高度, #1E1E1E 背景)
├── _mainLayout (QHBoxLayout, margins=0, spacing=0)
│   ├── leftLayout
│   │   ├── _iconLabel       (24×24 窗口图标, AlignCenter)
│   │   ├── _titleLabel      (标题文本, color:#e0e0e0, font-size:12px)
│   │   ├── _leftArea        (自定义左侧控件区域, 默认隐藏)
│   │   └── stretch
│   ├── _middleArea          (中间弹性区域, stretch=1, 默认隐藏)
│   └── rightLayout
│       ├── stretch
│       ├── _rightArea       (自定义右侧控件区域, 放_pinBtn等)
│       ├── _minButton       (最小化, hover背景: #3a3a4a)
│       ├── _maxButton       (最大化/还原, hover背景: #3a3a4a)
│       └── _closeButton     (关闭, hover背景: #e81123 红色)
```

#### 按钮尺寸自适应算法

```cpp
int btnW  = _barHeight + 4;                      // 36px 高 → 40px 宽, 48px 高 → 52px 宽
int iconSz = qBound(14, _barHeight - 12, 28);    // 36px 高 → 24px 图标, 48px 高 → 28px 图标
```

### 3.5 核心实现详解

#### ① `WM_NCCALCSIZE` — 扩展客户区覆盖标题栏

```cpp
// TitleBar.cpp:441-479
bool TitleBar::handleWinNcCalcSize(void* message, qintptr* result)
{
    NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
    RECT* clientRect = &params->rgrc[0];

    const LONG originTop = clientRect->top;        // 1. 保存原始顶部位置

    ::DefWindowProcW(hwnd, WM_NCCALCSIZE, wParam, lParam);  // 2. 让系统先算非客户区

    // 3. 关键：恢复 clientRect->top 到窗口原始顶部
    //    系统已经把 clientRect->top 下移了（留出标题栏空间），
    //    我们把它推回去——覆盖系统标题栏区域
    if (::IsZoomed(hwnd)) {
        clientRect->top = originTop;   // 最大化时 DefWindowProcW 已裁剪边框
    } else {
        clientRect->top = originTop;   // 正常状态恢复顶部
    }

    *result = WVR_REDRAW;
    return true;
}
```

**执行流程**：

1. 保存 `clientRect->top`（窗口原始顶部坐标）
2. 调用 `DefWindowProcW` 让系统做标准非客户区计算（**对 Snap Layouts 至关重要**——系统内部记录正确的几何信息）
3. 把 `clientRect->top` **恢复**到窗口原始顶部，只改这一个值，左右下保持 `DefWindowProcW` 的计算
4. 最大化时同理——`DefWindowProcW` 已经正确裁剪了边框

#### ② `WM_NCHITTEST` — 命中测试（告诉系统鼠标在窗口的哪个部位）

```cpp
// TitleBar.cpp:482-547
bool TitleBar::handleWinNcHitTest(void* message, qintptr* result)
{
    // Step 1: 最大化按钮 → HTZOOM (让 Windows 显示 Snap 弹出菜单)
    if (_maxButton->isVisible() && containsCursor(_maxButton)) {
        // 手动管理 hover 状态（HTZOOM 导致 Qt 收不到鼠标事件）
        _maxButton->setProperty("hovered", true);
        _maxButton->style()->polish(_maxButton);
        *result = HTZOOM;
        return true;
    }

    // Step 2: 窗口边缘 (8px 边框) → 调整大小区域
    if (!_fixedSize && !isMaximized) {
        if      (left && top)    *result = HTTOPLEFT;     // 左上角
        else if (left && bottom) *result = HTBOTTOMLEFT;  // 左下角
        else if (right && top)   *result = HTTOPRIGHT;    // 右上角
        else if (right && bottom)*result = HTBOTTOMRIGHT; // 右下角
        else if (left)           *result = HTLEFT;        // 左边框
        else if (right)          *result = HTRIGHT;       // 右边框
        else if (top)            *result = HTTOP;         // 上边框
        else if (bottom)         *result = HTBOTTOM;      // 下边框
    }

    // Step 3: TitleBar 空白区域 → HTCAPTION (可拖拽移动窗口)
    if (containsCursor(this) && !_targetWindow->isFullScreen()) {
        *result = HTCAPTION;
        return true;
    }

    // Step 4: 其余区域 → HTCLIENT (客户区，由 Qt 处理)
    *result = HTCLIENT;
    return true;
}
```

**`containsCursor()` 的智能排除逻辑**：

```cpp
// TitleBar.cpp:687-719
bool TitleBar::containsCursor(QWidget* widget) const
{
    if (widget == this) {
        // 1. 排除窗口控制按钮（最小化/最大化/关闭）
        for (QWidget* child : clientWidgets()) {
            if (containsCursor(child))
                return false;
        }
        // 2. 排除自定义区域中可交互的子控件
        //    只排除 focusPolicy != NoFocus 的控件（按钮、输入框等）
        //    标签、空白区域等仍可拖拽
        for (QWidget* area : {_leftArea, _middleArea, _rightArea}) {
            if (area->isVisible()) {
                for (QWidget* child : area->findChildren<QWidget*>(...)) {
                    if (child->isVisible()
                        && child->focusPolicy() != Qt::NoFocus   // 可交互控件
                        && containsCursor(child))
                        return false;
                }
            }
        }
    }
    // 坐标比较
    QPoint globalPos = QCursor::pos();
    QRect widgetRect(widget->mapToGlobal(QPoint(0, 0)), widget->size());
    return widgetRect.contains(globalPos);
}
```

#### ③ 最大化按钮的特殊处理 —— HTZOOM

最大化按钮返回 `HTZOOM` 而非 `HTMAXBUTTON`：

| 行为 | `HTZOOM` | `HTMAXBUTTON` |
|------|----------|---------------|
| 点击最大化 | ✅ | ✅ |
| Snap Layouts 菜单 | ✅ 弹出 | ❌ 不弹出 |
| Qt 鼠标事件 | ❌ 被 Windows 接管 | ✅ 正常 |

**代价与对策**：

```cpp
// hover 状态手动管理（HTZOOM 导致 Qt 收不到鼠标事件）
if (!_hoverMaxButton) {
    _hoverMaxButton = true;
    _maxButton->setProperty("hovered", true);
    _maxButton->style()->unpolish(_maxButton);
    _maxButton->style()->polish(_maxButton);
}

// WM_NCLBUTTONDOWN 拦截：不让 Windows 处理最大化按钮
bool TitleBar::handleWinNcLButtonDown(void*, qintptr*) {
    if (_maxButton->isVisible() && containsCursor(_maxButton))
        return true;  // 拦截
    return false;
}

// WM_NCLBUTTONUP 手动触发最大化/还原
bool TitleBar::handleWinNcLButtonUp(void* message, qintptr*) {
    if (_maxButton->isVisible() && containsCursor(_maxButton)) {
        WPARAM cmd = _targetWindow->isMaximized() ? SC_RESTORE : SC_MAXIMIZE;
        ::PostMessageW(hwnd, WM_SYSCOMMAND, cmd, 0);  // 异步，避免重入
        return true;
    }
    return false;
}
```

用 `PostMessage` 而非 `SendMessage` 发送 `WM_SYSCOMMAND`，避免消息处理栈中重入窗口状态变更引发的问题。

#### ④ `applyWinWindowStyle()` — DWM 集成

```cpp
// TitleBar.cpp:397-421
void TitleBar::applyWinWindowStyle()
{
    HWND hwnd = reinterpret_cast<HWND>(_targetWindow->winId());

    DWORD style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
    style &= ~WS_SYSMENU;  // 移除系统菜单，保留 THICKFRAME + MAXIMIZEBOX
    ::SetWindowLongPtrW(hwnd, GWL_STYLE, style);

    // DwmExtendFrameIntoClientArea 设为全 0 = 由我们通过 WM_NCCALCSIZE 管理
    MARGINS margins = {0, 0, 0, 0};
    ::DwmExtendFrameIntoClientArea(hwnd, &margins);

    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                   SWP_NOZORDER | SWP_NOOWNERZORDER |
                   SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
}
```

- **移除 `WS_SYSMENU`**：禁用默认系统菜单（左上角图标右键菜单），用自定义图标/标题替代
- **`DwmExtendFrameIntoClientArea(全0)`**：告诉 DWM "非客户区由我自己通过 `WM_NCCALCSIZE` 管理"
- **`SWP_FRAMECHANGED`**：通知系统重新计算窗口框架

#### ⑤ `updateWinWindowMargins()` — 最大化时隐藏 resize 边框

```cpp
// TitleBar.cpp:423-438
void TitleBar::updateWinWindowMargins(bool maximized)
{
    if (maximized) {
        _resizeMargin = 0;   // 最大化时边框被裁剪，不需要 resize 边距
    } else {
        _resizeMargin = 8;   // 正常状态 8px 边框可拖拽调整大小
    }
    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                   SWP_NOZORDER | SWP_NOOWNERZORDER |
                   SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
}
```

#### ⑥ `WM_GETMINMAXINFO` — 窗口最小尺寸约束

```cpp
// TitleBar.cpp:562-582
bool TitleBar::handleWinGetMinMaxInfo(void* message, qintptr* result)
{
    MINMAXINFO* minmax = reinterpret_cast<MINMAXINFO*>(lParam);

    // 基于可见控件动态计算最小宽度
    int minWidth = 0;
    if (_iconLabel->isVisible())  minWidth += 24 + 4;
    if (_titleLabel->isVisible()) minWidth += _titleLabel->minimumWidth() + 4;
    if (_minButton->isVisible())  minWidth += 46;
    if (_maxButton->isVisible())  minWidth += 46;
    if (_closeButton->isVisible())minWidth += 46;
    minWidth = qMax(minWidth, 200);                    // 不小于 200px

    minmax->ptMinTrackSize.x = minWidth;
    minmax->ptMinTrackSize.y = _barHeight * 2;         // 最小高度至少 2 倍标题栏高度

    return true;
}
```

### 3.6 跨平台兼容

```cpp
// TitleBar.cpp:613-681 — 非 Windows 平台走 Qt 原生方式
#ifndef Q_OS_WIN
void TitleBar::handleMouseEvents(QObject* obj, QEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        if (_edges != 0)
            _targetWindow->windowHandle()->startSystemResize(_edges);  // 调整大小
        else if (containsCursor(this))
            _targetWindow->windowHandle()->startSystemMove();           // 拖拽移动
        break;

    case QEvent::MouseButtonDblClick:
        if (containsCursor(this) && !_fixedSize)
            // 双击切换最大/还原
            break;

    case QEvent::HoverMove:
        // 根据鼠标位置设置 _edges 和光标样式
        if (p.x() < _resizeMargin)           _edges |= Qt::LeftEdge;
        if (p.x() > width() - _resizeMargin) _edges |= Qt::RightEdge;
        if (p.y() < _resizeMargin)           _edges |= Qt::TopEdge;
        if (p.y() > height() - _resizeMargin)_edges |= Qt::BottomEdge;
        // 设置对应的 SizeHorCursor / SizeVerCursor / SizeFDiagCursor ...
        break;
    }
}
#endif
```

非 Windows 平台才使用 `Qt::FramelessWindowHint`（见 `setupWindow()` 的 `#else` 分支），拖拽和调整大小通过 Qt 的 `startSystemMove()` / `startSystemResize()` 实现。

### 3.7 固定窗口大小

```cpp
// TitleBar.cpp:166-186
void TitleBar::setFixedWindowSize(bool fixed)
{
    _fixedSize = fixed;
    _maxButton->setVisible(!fixed);  // 固定大小则隐藏最大化按钮

#ifdef Q_OS_WIN
    // 动态移除/添加 WS_THICKFRAME + WS_MAXIMIZEBOX 样式位
    HWND hwnd = reinterpret_cast<HWND>(_targetWindow->winId());
    DWORD style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
    if (fixed) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);  // 移除 resize 和最大化
    } else {
        style |= (WS_THICKFRAME | WS_MAXIMIZEBOX);   // 恢复
    }
    ::SetWindowLongPtrW(hwnd, GWL_STYLE, style);
    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
#endif
}
```

### 3.8 绘制

```cpp
// TitleBar.cpp:381-389
void TitleBar::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 底部一条细线作为视觉分隔
    painter.setPen(QColor(0x30, 0x30, 0x40));
    painter.drawLine(0, height() - 1, width(), height() - 1);
}
```

---

## 4. MainWin 中的消息路由

```cpp
// mainwin.cpp:113-204 — MainWin::nativeEvent()
bool MainWin::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    if (eventType != "windows_generic_MSG" || !message)
        return QMainWindow::nativeEvent(eventType, message, result);

    const auto msg = static_cast<const MSG*>(message);

    switch (msg->message)
    {
    case WM_NCCALCSIZE:                              // ← 核心1: 客户区扩展
        return _titleBar->handleWinNcCalcSize(message, result);

    case WM_NCHITTEST:                               // ← 核心2: 命中测试
        return _titleBar->handleWinNcHitTest(message, result);

    case WM_SIZE:                                    // 跟踪最大/还原状态
        _titleBar->handleWinSize(message);
        break;

    case WM_GETMINMAXINFO:                           // 最小尺寸
        _titleBar->handleWinGetMinMaxInfo(message, result);
        return true;

    case WM_NCLBUTTONDOWN:                           // 拦截最大化按钮
        if (_titleBar->handleWinNcLButtonDown(message, result))
            return true;
        break;

    case WM_NCLBUTTONUP:                             // 手动触发最大/还原
        if (_titleBar->handleWinNcLButtonUp(message, result))
            return true;
        break;

    case WM_NCLBUTTONDBLCLK:                         // 双击标题栏
        if (_titleBar->handleWinNcLButtonDown(message, result))
            return true;
        break;

    case WM_NCPAINT:                                 // DWM 开启时跳过非客户区绘制
        if (::DwmIsCompositionEnabled(nullptr) == S_OK) {
            *result = FALSE;
            return true;
        }
        break;

    case WM_NCACTIVATE:                              // DWM 处理激活/失活
        if (::DwmIsCompositionEnabled(nullptr) == S_OK) {
            *result = ::DefWindowProcW(hwnd, WM_NCACTIVATE, msg->wParam, -1);
        } else {
            *result = TRUE;
        }
        return true;

    case WM_SHOWWINDOW:                              // 首次显示时刷新框架
        if (msg->wParam == TRUE) {
            ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
        }
        break;
    }

    return QMainWindow::nativeEvent(eventType, message, result);
}
```

---

## 5. 其他值得关注的设计点

### 5.1 置顶按钮

```cpp
// mainwin.cpp:48-72
_pinBtn = new QToolButton(this);
_pinBtn->setToolTip(tr("置顶窗口"));
_pinBtn->setIcon(QIcon(":/image/ding1.png"));
connect(_pinBtn, &QToolButton::clicked, this, [this]() {
    _isTop = !_isTop;
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
});
_titleBar->addRightWidget(_pinBtn);
```

### 5.2 线程模型

通信、PID、OSC 各自运行在独立的 `QThread` 中，通过信号/槽与主线程通信：

```
主线程 (Widget)
  ├── Com 线程      → startComThread / closeComThread
  ├── Pid 线程      → startPidThread / closePidThread
  ├── OSC 线程      → startOscThread / closeOscThread
  └── Download 线程  → sendDowCmd / closeDowThread
```

使用 `qRegisterMetaType` 注册自定义类型用于跨线程信号/槽传递。

### 5.3 样式系统

- **全局样式**：`center.css`，通过 `Widget::loadStyleSheet(":/style/center.css")` 加载
- **TitleBar 自身样式**：硬编码在 `initUI()` 中（`#TitleBar{background-color:#1E1E1E;}`）
- **窗口按钮样式**：内联在 `initUI()` 中，hover 颜色区分普通按钮和关闭按钮
- **暗色主题**：全局背景 `#1E1E1E`，文字 `#DCDCDC`，强调色 `#23A7F2`

---

## 6. Windows 消息处理全景图

```
                         ┌──────────────────────────┐
用户操作                  │     Windows DWM / UX     │        最终效果
                         └──────────┬───────────────┘
                                    │ WM_xxx 消息
                                    ▼
                         ┌──────────────────────────┐
                         │   MainWin::nativeEvent   │
                         │   (消息路由器)            │
                         └──────────┬───────────────┘
                                    │
          ┌─────────────────────────┼──────────────────────────┐
          │                         │                          │
          ▼                         ▼                          ▼
┌─────────────────────┐  ┌─────────────────────┐  ┌──────────────────────┐
│  WM_NCCALCSIZE      │  │  WM_NCHITTEST       │  │  其他消息             │
│  → handleWinNcCalc  │  │  → handleWinNcHit   │  │  (SIZE, ACTIVATE,    │
│    Size()           │  │    Test()           │  │   NCPAINT, SHOW...)  │
│                     │  │                     │  │                      │
│ 客户区顶到窗口顶部   │  │ 精确命中测试:        │  │ DWM / Snap 兼容     │
│ 覆盖系统标题栏       │  │  • HTZOOM  Snap菜单  │  │                      │
│                     │  │  • HTCAPTION 拖拽    │  │                      │
│                     │  │  • HTTOPLEFT.. 缩放  │  │                      │
│                     │  │  • HTCLIENT  客户区  │  │                      │
└─────────────────────┘  └─────────────────────┘  └──────────────────────┘
```

---

## 7. 总结：TitleBar 实现亮点

| 特性 | 实现方式 | 好处 |
|------|----------|------|
| **保留 Snap Layouts** | 不设 `FramelessWindowHint`，保留 `WS_THICKFRAME` | 窗口拖到屏幕边缘正常工作 |
| **覆盖系统标题栏** | `WM_NCCALCSIZE` 推 `clientRect->top` 到窗口顶部 | 用 QWidget 替代系统标题栏 |
| **精确拖拽区域** | `WM_NCHITTEST` 返回 `HTCAPTION`，排除可交互控件 | 标题栏空白区域可拖拽，按钮可点击 |
| **Snap 弹出菜单** | 最大化按钮返回 `HTZOOM` | 鼠标悬停最大化按钮弹出布局菜单 |
| **DWM 集成** | `DwmExtendFrameIntoClientArea(0)` 自己管理非客户区 | 阴影、动画等 DWM 效果正常 |
| **动态固定窗口** | 运行时增删 `WS_THICKFRAME` / `WS_MAXIMIZEBOX` | 无需重建窗口即可切换 |
| **跨平台** | Windows 走 Native API，其他平台走 Qt 原生 | macOS / Linux 也能工作 |
| **可定制性** | 左/中/右三个自定义控件区域 + 按钮显隐 + 高度可变 | 灵活扩展 |
| **最小尺寸约束** | `WM_GETMINMAXINFO` 动态计算最小宽高 | 窗口不会缩到控件变形 |
