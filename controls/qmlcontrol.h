﻿#ifndef QMLCONTROL_H
#define QMLCONTROL_H

#include "ShowBoard_global.h"
#include "core/widgetcontrol.h"

class SHOWBOARD_EXPORT QmlControl : public WidgetControl
{
    Q_OBJECT
public:
    Q_INVOKABLE QmlControl(ResourceView *res);

protected:
    virtual QWidget * createWidget(ResourceView * res) override;

private slots:
    void hide();

    void update();
};

#endif // QMLCONTROL_H
