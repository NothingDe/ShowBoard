#ifndef VIDEOCONTROL_H
#define VIDEOCONTROL_H

#include "core/control.h"

class QMediaPlayer;

class VideoControl : public Control
{
    Q_OBJECT

    Q_PROPERTY(qreal playRate READ playRate WRITE setPlayRate)

public:
    Q_INVOKABLE VideoControl(ResourceView *res);

public:
    qreal playRate() const;

    void setPlayRate(qreal v);

public slots:
    void play();

    void stop();

protected:
    virtual QGraphicsItem * create(ResourceView * res) override;

    virtual void attached() override;

    virtual void detached() override;

    virtual void resize(const QSizeF &size) override;

    virtual void updateToolButton(ToolButton * button) override;

private:
    QMediaPlayer * player_;
};

#endif // VIDEOCONTROL_H
