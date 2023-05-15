#include "tille.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QDebug>

#ifdef Q_OS_WINDOWS
#include <windows.h>
#endif

TilleBar::TilleBar(QWidget *parent)
    : QWidget(parent)
{
    this->setParent(parent);
    this->setCursor(Qt::SizeAllCursor); //更改鼠标指针样式

    //添加一个水平布局框
    horizontalLayout = new QHBoxLayout();
    horizontalLayout->setSpacing(0);
    horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
    horizontalLayout->setContentsMargins(0, 0, 0, 0);

    //图标
    icon = new QLabel(this);
    icon->setObjectName(QString::fromUtf8("icon"));
    icon->setMinimumSize(QSize(32, 32));
    icon->setMaximumSize(QSize(32, 32));
    horizontalLayout->addWidget(icon);

    //标题
    tille = new QLabel(this);
    tille->setObjectName(QString::fromUtf8("tille"));
    tille->setMaximumHeight(32);
    horizontalLayout->addWidget(tille);

    horizontalSpacer = new QSpacerItem(637, 28, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalLayout->addItem(horizontalSpacer);

    //指定窗口
    pinBtn = new QToolButton(this);
    pinBtn->setCursor(Qt::PointingHandCursor);
    pinBtn->setObjectName(QString::fromUtf8("pinBtn"));
    pinBtn->setMinimumSize(QSize(32, 32));
    pinBtn->setMaximumSize(QSize(32, 32));
    pinBtn->setIcon(QIcon(":/image/ding1.png"));
    horizontalLayout->addWidget(pinBtn);

    //最小化按钮
    minBtn = new QToolButton(this);
    minBtn->setCursor(Qt::PointingHandCursor);
    minBtn->setObjectName(QString::fromUtf8("minBtn"));
    minBtn->setMinimumSize(QSize(32, 32));
    minBtn->setMaximumSize(QSize(32, 32));
    minBtn->setIcon(QIcon(":/image/min.png"));
    horizontalLayout->addWidget(minBtn);

    //最大化按钮
    maxBtn = new QToolButton(this);
    maxBtn->setCursor(Qt::PointingHandCursor);
    maxBtn->setObjectName(QString::fromUtf8("maxBtn"));
    maxBtn->setMinimumSize(QSize(32, 32));
    maxBtn->setMaximumSize(QSize(32, 32));
    maxBtn->setIcon(QIcon(":/image/max.png"));
    horizontalLayout->addWidget(maxBtn);

    //关闭按钮
    closeBtn = new QToolButton(this);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setObjectName(QString::fromUtf8("closeBtn"));
    closeBtn->setMinimumSize(QSize(32, 32));
    closeBtn->setMaximumSize(QSize(32, 32));
    closeBtn->setIcon(QIcon(":/image/close.png"));
    horizontalLayout->addWidget(closeBtn);

    this->setLayout(horizontalLayout);
    this->setMaximumHeight(32);

    connect(pinBtn, &QPushButton::clicked, this, [=]()
    {
        isTop = !isTop;
        if (isTop)
        {
#ifdef Q_OS_WINDOWS
            //调用windows api 置顶层
            ::SetWindowPos((HWND)this->parentWidget()->winId(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            pinBtn->setIcon(QIcon(":/image/ding2.png"));
#endif
        }
        else
        {
#ifdef Q_OS_WINDOWS
            //调用windows api 取消置顶层
            ::SetWindowPos((HWND)this->parentWidget()->winId(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            pinBtn->setIcon(QIcon(":/image/ding1.png"));
#endif
        }
    });
    connect(minBtn, &QPushButton::clicked, this, [=]()
    {
        this->parentWidget()->showMinimized();
    });
    connect(maxBtn, &QPushButton::clicked, this, [=]()
    {
        isMax = !isMax;
        if (isMax) //最大化
        {
            maxBtn->setIcon(QIcon(":/image/recover.png"));
            location = this->parentWidget()->geometry();
            this->parentWidget()->setGeometry(qApp->desktop()->availableGeometry());
        }
        else    //复原
        {
            maxBtn->setIcon(QIcon(":/image/max.png"));
            this->parentWidget()->setGeometry(location);
        }
    });

    connect(closeBtn, &QPushButton::clicked, this, [=]()
    {
        this->parentWidget()->close();
    });
}

TilleBar::~TilleBar()
= default;

/**
 * @brief 设置图标
 * @param fileName 图标文件路径
 */
void TilleBar::setIcon(const QString &fileName)
{
    QIcon icon_pic = QIcon(fileName);
    QPixmap pic = icon_pic.pixmap(icon_pic.actualSize(QSize(28, 28)));//size自行调整
    icon->setPixmap(pic);
}

/**
 * @brief 设置标题
 * @param text 文本
 */
void TilleBar::setTille(const QString &text)
{
    tille->setText(text);
}

/**
 * @brief 加载样式表
 * @param styleSheetFile 样式表文件路径
 */
void TilleBar::loadStyleSheet(const QString& styleSheetFile)
{
    QFile file(styleSheetFile);
    file.open(QFile::ReadOnly);
    if (file.isOpen())
    {
        QString styleSheet = this->styleSheet();
        styleSheet += QLatin1String(file.readAll());//读取样式表文件
        this->setStyleSheet(styleSheet);
        file.close();
    }
}

/**
 * @brief 双击放大或者缩小
 * @param event 事件
 */
void TilleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    maxBtn->click();
    isDoubleClick = true;
    QWidget::mouseDoubleClickEvent(event);
}

/**
 * @brief 鼠标按下事件
 * @param event 事件
 */
void TilleBar::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        isPressed = true;
        startMovePos = event->globalPos();
    }
    QWidget::mousePressEvent(event);
}

/**
 * @brief 鼠标释放事件
 * @param event 事件
 */
void TilleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (isPressed)
    {
        QPoint mousePoint = event->globalPos();
        if (isMax && !isDoubleClick)    //最大时拖动复原
        {
            double proportion;
            proportion = (double)mousePoint.x()/(double)this->parentWidget()->width();
            maxBtn->click();
            this->parentWidget()->move(mousePoint.x() - (int)(proportion * this->parentWidget()->width()), mousePoint.y() - 16);
        }
        else    //拖动主窗口
        {
            QPoint movePoint = mousePoint - startMovePos;
            QPoint widgetPos = this->parentWidget()->pos();
            startMovePos = mousePoint;
            this->parentWidget()->move(widgetPos.x() + movePoint.x(), widgetPos.y() + movePoint.y());
        }
        isDoubleClick = false;
        event->accept();
    }
    else
    {
        QWidget::mouseMoveEvent(event);
    }
}

/**
 * @brief 鼠标释放事件
 * @param event 事件
 */
void TilleBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        isPressed = false;
    }
    QWidget::mouseReleaseEvent(event);
}

