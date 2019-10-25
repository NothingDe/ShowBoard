#include "toolbarwidget.h"
#include "toolbutton.h"

#include <QHBoxLayout>
#include <QToolButton>
#include <QPainter>
#include <QWidget>
#include <QStyleOptionButton>
#include <QGraphicsItem>

ToolbarWidget::ToolbarWidget(QWidget *parent)
    : QWidget(parent)
{
    layout_ = new QHBoxLayout(this);
    style_ = new QStyleOptionButton();
    style_->features = QStyleOptionButton::Flat;
}

static QPixmap widgetToPixmap(QWidget * widget)
{
    QPixmap pm(widget->size());
    QPainter pt(&pm);
    widget->render(&pt);
    return pm;
}

static QPixmap itemToPixmap(QGraphicsItem * item)
{
    QPixmap pm(item->boundingRect().size().toSize());
    QPainter pt(&pm);
    QStyleOptionGraphicsItem style;
    item->paint(&pt, &style);
    return pm;
}

void ToolbarWidget::setToolButtons(QList<ToolButton *> const & buttons)
{
    clear();
    for (ToolButton * b : buttons) {
        addToolButton(b);
    }
    layout_->update();
}

void ToolbarWidget::setToolButtons(ToolButton buttons[], int count)
{
    clear();
    for (int i = 0; i < count; ++i) {
        addToolButton(buttons + i);
    }
    layout_->update();
}

void ToolbarWidget::addToolButton(ToolButton * button)
{
    QToolButton * btn = new QToolButton();
    btn->setIconSize({40, 40});
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    QVariant & icon = button->icon;
    if (icon.type() == QVariant::String)
        btn->setIcon(QIcon(icon.toString()));
    else if (icon.type() == QVariant::Pixmap)
        btn->setIcon(QIcon(icon.value<QPixmap>()));
    else if (icon.type() == QVariant::Image)
        btn->setIcon(QIcon(QPixmap::fromImage(icon.value<QImage>())));
    else if (icon.type() == QVariant::UserType) {
        if (icon.userType() == QMetaType::QObjectStar)
            btn->setIcon(QIcon(widgetToPixmap(icon.value<QWidget *>())));
        else if (icon.userType() == qMetaTypeId<QGraphicsItem *>())
            btn->setIcon(QIcon(itemToPixmap(icon.value<QGraphicsItem *>())));
    }
    btn->setText(button->title);
    void (ToolbarWidget::*slot)() = &ToolbarWidget::buttonClicked;
    QObject::connect(btn, &QToolButton::clicked, this, slot);
    layout_->addWidget(btn);
    buttons_.insert(btn, button);
}

void ToolbarWidget::clear()
{
    for (QWidget * w : buttons_.keys()) {
        layout_->removeWidget(w);
        w->deleteLater();
    }
    buttons_.clear();
}

void ToolbarWidget::buttonClicked()
{
    QWidget * btn = qobject_cast<QWidget *>(sender());
    ToolButton * b = buttons_.value(btn);
    if (b)
        emit buttonClicked(b);
}
