#include "mainwin.h"
#include <QApplication>
#include <QDebug>
#include <QPainter>

#define MARGIN 5

/**
 * @brief 构造函数
 * @param parent 父窗口
 */
MainWin::MainWin(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint);    //无边框
    setAttribute(Qt::WA_TranslucentBackground); //窗口透明
    this->setWindowIcon(QIcon(":/image/logo.png"));

    layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setObjectName(QString::fromUtf8("layout"));
    layout->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);

    tilleBar = new TilleBar(this);
    tilleBar->setIcon(":/image/logo.png");
    tilleBar->setTille(QString::fromUtf8("多功能调试助手"));
    tilleBar->loadStyleSheet(":/style/tille.css");
    layout->addWidget(tilleBar);

    centerWidget = new Widget(this);
    centerWidget->loadStyleSheet(":/style/center.css");
    layout->addWidget(centerWidget);

    this->setMouseTracking(true);
    this->setMinimumSize(QSize(1038, 844));
}

/**
 * @brief 绘制事件，绘制圆角
 * @param event 事件
 */
void MainWin::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    //边缘进行抗锯齿处理
    painter.setRenderHint(QPainter::Antialiasing);
    //笔刷颜色
    painter.setBrush(QColor(30,30,30));
    // Pen 用于绘制轮廓线
    painter.setPen(Qt::transparent);
    // 获取绘制区域
    QRect rect = this->rect();
    // 设置绘制区域宽度
    rect.setWidth(rect.width());
    // 设置绘制区域高度
    rect.setHeight(rect.height());
    //绘制轮廓
    painter.drawRoundedRect(rect, 7, 7);

    QWidget::paintEvent(event);
}

/**
 * @brief 鼠标按下事件
 * @param event
 */
void MainWin::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        isleftpressed = true;
        plast = event->globalPos();
        curpos = getRegion(event->pos());
    }

    QWidget::mousePressEvent(event);
}

/**
 * @brief 鼠标释放事件
 * @param event 事件
 */
void MainWin::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        isleftpressed = false;
    }
}

/**
 * @brief 鼠标移动事件
 * @param event 事件
 */
void MainWin::mouseMoveEvent(QMouseEvent *event)
{
    int region = getRegion(event->pos());
    setCursorType(region);
    if (isleftpressed)
    {
        QPoint ptemp = event->globalPos();
        ptemp = ptemp - plast;
        QRect qRect = geometry();
        QRect qRectBack = geometry();
        switch (curpos)//改变窗口的大小
        {
        case 11:    //左上角
            qRect.setTopLeft(qRect.topLeft() + ptemp);
            break;
        case 13:    //右上角
            qRect.setTopRight(qRect.topRight() + ptemp);
            break;
        case 31:    //左下角
            qRect.setBottomLeft(qRect.bottomLeft() + ptemp);
            break;
        case 33:    //右下角
            qRect.setBottomRight(qRect.bottomRight() + ptemp);
            break;
        case 12:    //中上角
            qRect.setTop(qRect.top() + ptemp.y());
            break;
        case 21:    //中左角
            qRect.setLeft(event->globalPos().x());
            break;
        case 23:    //中右角
            qRect.setRight(qRect.right() + ptemp.x());
            break;
        case 32:    //中下角
            qRect.setBottom(qRect.bottom() + ptemp.y());
            break;
        }
        //判断是否窗口过小
        if (qRect.width() > minimumWidth() && qRect.height() > minimumHeight())
        {
            qRectBack = qRect;
        }
        setGeometry(qRectBack);
        plast = event->globalPos();
    }
    QWidget::mouseMoveEvent(event);
}

/**
 * @brief 获取鼠标所在区域
 * @param p 鼠标坐标
 * @return 区域
 */
int MainWin::getRegion(QPoint p)
{
    int row = (p.x()<MARGIN) ? 1 : (p.x()>(this->width() - MARGIN) ? 3 : 2);

    if(p.y() < MARGIN)
        return 10+row;
    else if(p.y() > this->height()-MARGIN)
        return 30+row;
    else
        return 20+row;
}

/**
 * @brief 根据区域设置鼠标指针形状
 * @param region 区域
 */
void MainWin::setCursorType(int region)
{
    switch(region)
    {
    case 11:
    case 33:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case 13:
    case 31:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case 21:
    case 23:
        setCursor(Qt::SizeHorCursor);
        break;
    case 12:
    case 32:
        setCursor(Qt::SizeVerCursor);
        break;
    default:
        setCursor(Qt::ArrowCursor);
        QApplication::restoreOverrideCursor();//恢复鼠标指针性状
        break;
    }
}
