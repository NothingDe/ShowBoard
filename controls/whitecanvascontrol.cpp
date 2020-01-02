#include "whitecanvascontrol.h"
#include "views/whitecanvas.h"
#include "core/resourceview.h"
#include "core/resourcetransform.h"

#include <QUrl>
#include <QGraphicsScene>
#include <QtMath>
#include <QDebug>

WhiteCanvasControl::WhiteCanvasControl(ResourceView * view, QGraphicsItem * canvas)
    : Control(view, {}, {CanSelect, CanRotate})
{
    item_ = canvas;
    item_->setTransformations({transform_});
    item_->setData(ITEM_KEY_CONTROL, QVariant::fromValue(this));
    realItem_ = item_;
    QObject::connect(&res_->transform(), &ResourceTransform::beforeChanged,
                     this, &WhiteCanvasControl::updatingTransform);
    QObject::connect(&res_->transform(), &ResourceTransform::changed,
                     this, &WhiteCanvasControl::updateTransform);
    loadSettings();
    posBar_ = new PositionBar(canvas);
    qDebug() << "WhiteCanvasControl" << res_->transform().transform();
}

WhiteCanvasControl::~WhiteCanvasControl()
{
    qDebug() << "~WhiteCanvasControl" << res_->transform().transform();
    delete posBar_;
    saveSettings();
    item_->setData(ITEM_KEY_CONTROL, QVariant());
    item_->setTransformations({});
    item_ = nullptr;
    realItem_ = nullptr;
}

QGraphicsItem *WhiteCanvasControl::create(ResourceView *res)
{
    (void) res;
    return nullptr;
}

void WhiteCanvasControl::resize(const QSizeF &size)
{
    qDebug() << "WhiteCanvasControl resize" << size;
    QRectF rect(QPointF(0, 0), size);
    rect.moveCenter(QPointF(0, 0));
    rect |= item_->boundingRect();
    static_cast<WhiteCanvas*>(item_)->setGeometry(rect);
}

void WhiteCanvasControl::sizeChanged()
{
    // do nothing
}

void WhiteCanvasControl::updatingTransform()
{
    ResourceTransform & transform = res_->transform();
    QRectF srect = item_->scene()->sceneRect();
    QRectF crect = item_->boundingRect();
    transform.keepOuterOf(srect, crect);
}

void WhiteCanvasControl::updateTransform()
{
    ResourceTransform & transform = res_->transform();
    QRectF srect = item_->scene()->sceneRect();
    QRectF crect = item_->boundingRect();
    posBar_->update(srect, crect, transform.scale().m11(), transform.offset());
}


PositionBar::PositionBar(QGraphicsItem * parent)
    : QGraphicsPathItem(parent)
{
    setPen(Qt::NoPen);
    setBrush(Qt::blue);
}

void PositionBar::update(const QRectF &viewRect, const QRectF &canvasRect, qreal scale, QPointF offset)
{
    constexpr qreal WIDTH = 8;
    QPointF pos = (viewRect.topLeft() - offset) - canvasRect.topLeft() * scale;
    QSizeF scl(viewRect.width() / canvasRect.width() / scale, viewRect.height() / canvasRect.height() / scale);
    QRectF vRect(viewRect.right() - WIDTH, pos.y() * scl.height() + viewRect.top(),
                 WIDTH, viewRect.height() * scl.height());
    QRectF hRect(pos.x() * scl.width() + viewRect.left(), viewRect.bottom() - WIDTH,
                 viewRect.width() * scl.width(), WIDTH);
    QPainterPath ph;
    if (!qFuzzyCompare(vRect.height(), viewRect.height())) {
        vRect.translate(-offset);
        vRect = QRectF(vRect.x() / scale, vRect.y() / scale, vRect.width() / scale, vRect.height() / scale);
        ph.addRoundedRect(vRect, WIDTH / 2, WIDTH / 2);
    }
    if (!qFuzzyCompare(hRect.width(), viewRect.width())) {
        hRect.translate(-offset);
        hRect = QRectF(hRect.x() / scale, hRect.y() / scale, hRect.width() / scale, hRect.height() / scale);
        ph.addRoundedRect(hRect, WIDTH / 2, WIDTH / 2);
    }
    setPath(ph);
}
