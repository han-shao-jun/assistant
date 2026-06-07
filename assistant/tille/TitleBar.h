#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QWidget>

class QLabel;
class QHBoxLayout;
class QToolButton;

/**
 * @brief 自定义窗口标题栏
 *
 * 仿照 ElaWidgetTools 的 ElaAppBar，通过拦截 Windows 原生消息实现：
 * - WM_NCCALCSIZE: 将客户区扩展到标题栏区域
 * - WM_NCHITTEST:  自定义拖拽/调整大小/最大化按钮的命中区域
 *
 * 核心思想：
 *   保留 WS_THICKFRAME | WS_MAXIMIZEBOX（Snap Layouts 依赖这两个样式），
 *   通过 WM_NCCALCSIZE 把 clientRect->top 推到窗口顶部，
 *   让"原标题栏区域"变成客户区，然后在这里放自己的 TitleBar QWidget。
 *
 * 使用方式：
 *   MainWindow 构造时创建 TitleBar 并调用 setupWindow()。
 */
class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget* parent = nullptr);
    ~TitleBar() override;

    /// 将 TitleBar 安装到目标窗口上，拦截原生消息
    void setupWindow(QWidget* targetWindow);

    // ---- 标题文本 ----
    void setTitle(const QString& title);
    QString title() const;

    // ---- 窗口图标 ----
    void setWindowIcon(const QIcon& icon);

    // ---- 自定义控件区域 ----
    /// 添加到左侧区域（标题右侧）
    void addLeftWidget(QWidget* widget);
    /// 添加到中间区域（拉伸区域）
    void addMiddleWidget(QWidget* widget);
    /// 添加到右侧区域（窗口按钮左侧）
    void addRightWidget(QWidget* widget);

    // ---- 窗口按钮显隐 ----
    void setMinimizeButtonVisible(bool visible);
    void setMaximizeButtonVisible(bool visible);
    void setCloseButtonVisible(bool visible);

    // ---- 固定窗口大小（禁用resize和最大化） ----
    void setFixedWindowSize(bool fixed);

    // ---- 标题栏高度 ----
    void setBarHeight(int height);
    int barHeight() const;

    // ---- Windows 原生消息处理（公开，供 MainWindow::nativeEvent 调用） ----
#ifdef Q_OS_WIN
    bool handleWinNcCalcSize(void* message, qintptr* result);
    bool handleWinNcHitTest(void* message, qintptr* result);
    bool handleWinSize(void* message);
    bool handleWinGetMinMaxInfo(void* message, qintptr* result);
    bool handleWinNcLButtonDown(void* message, qintptr* result);
    bool handleWinNcLButtonUp(void* message, qintptr* result);
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

private:
    // ---- 内部布局 ----
    void initUI();

    // ---- 跨平台消息处理（非Windows走Qt） ----
    void handleMouseEvents(QObject* obj, QEvent* event);

    // ---- 辅助函数 ----
    /// 判断一个 widget 是否包含鼠标光标
    bool containsCursor(QWidget* widget) const;
    /// 更新最大化/还原按钮图标
    void updateMaxButtonIcon(bool isMaximized);
    /// 获取标题栏中所有可交互的子控件
    QList<QWidget*> clientWidgets() const;

    // ---- 布局 ----
    QHBoxLayout* _mainLayout = nullptr;

    // ---- 子控件 ----
    QLabel*      _iconLabel  = nullptr;
    QLabel*      _titleLabel = nullptr;
    QWidget*     _leftArea   = nullptr;   // 自定义左侧区域
    QWidget*     _middleArea = nullptr;   // 自定义中间区域
    QWidget*     _rightArea  = nullptr;   // 自定义右侧区域
    QToolButton* _minButton  = nullptr;
    QToolButton* _maxButton  = nullptr;
    QToolButton* _closeButton = nullptr;

    QHBoxLayout* _leftAreaLayout   = nullptr;
    QHBoxLayout* _middleAreaLayout = nullptr;
    QHBoxLayout* _rightAreaLayout  = nullptr;

    // ---- 状态 ----
    QWidget* _targetWindow  = nullptr;
    int      _barHeight     = 48;
    int      _resizeMargin  = 8;   // 边框可拖拽调整大小的像素宽度
    bool     _fixedSize     = false;
    bool     _hoverMaxButton = false;

    // ---- 跨平台拖拽/调整大小状态 ----
    int  _edges     = 0;
    bool _dragging  = false;
};

#endif // TITLEBAR_H
