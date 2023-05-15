#ifndef TILLE_H
#define TILLE_H

#include <QMainWindow>
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QIcon>
#include <QString>
#include <QPoint>
#include <QMouseEvent>

class TilleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TilleBar(QWidget *parent);
    ~TilleBar();
    void setIcon(const QString &fileName);
    void setTille(const QString &text);
    void loadStyleSheet(const QString& styleSheetFile);

private:
    QHBoxLayout *horizontalLayout;
    QLabel *icon;
    QLabel *tille;
    QSpacerItem *horizontalSpacer;
    QToolButton *pinBtn;
    QToolButton *minBtn;
    QToolButton *maxBtn;
    QToolButton *closeBtn;
    QPoint startMovePos;
    QRect location;
    bool isTop = false;
    bool isMax = false;
    bool isPressed = false;
    bool isDoubleClick = false;

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;


};
#endif // TILLE_H
