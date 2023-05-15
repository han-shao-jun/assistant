#ifndef MAINWIN_H
#define MAINWIN_H

#include <QWidget>
#include "tille.h"
#include <QVBoxLayout>
#include "widget.h"

class MainWin : public QWidget
{
    Q_OBJECT
public:
    explicit MainWin(QWidget *parent = nullptr);
    TilleBar *tilleBar;
    QVBoxLayout *layout;
    Widget *centerWidget;
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    int getRegion(QPoint p);        //获取光标在窗口所在区域
    void setCursorType(int region);  //根据传入的坐标，设置光标样式
    int curpos = 0;
    QPoint plast;
    bool isleftpressed = false;
};

#endif // MAINWIN_H
