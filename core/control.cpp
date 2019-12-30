#include "control.h"
#include "resource.h"
#include "resourceview.h"
#include "toolbutton.h"
#include "views/whitecanvas.h"
#include "views/stateitem.h"
#include "views/itemframe.h"
#include "views/itemselector.h"
#include "resourcetransform.h"
#include "controltransform.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QTransform>
#include <QGraphicsTransform>
#include <QMetaMethod>

#include <map>

Control * Control::fromItem(QGraphicsItem * item)
{
    return item->data(ITEM_KEY_CONTROL).value<Control *>();
}

ToolButton Control::btnTop = { "top", "置顶", nullptr, ":/showboard/icons/copy.svg" };
ToolButton Control::btnCopy = { "copy", "复制", nullptr, ":/showboard/icons/copy.svg" };
ToolButton Control::btnFastCopy = { "copy", "快速复制", nullptr, ":/showboard/icons/copy.svg" };
ToolButton Control::btnDelete = { "delete", "删除", nullptr, ":/showboard/icons/delete.svg" };

Control::Control(ResourceView *res, Flags flags, Flags clearFlags)
    : flags_((DefaultFlags | flags) & ~clearFlags)
    , res_(res)
    , transform_(nullptr)
    , item_(nullptr)
    , realItem_(nullptr)
    , stateItem_(nullptr)
{
    if (res_->flags().testFlag(ResourceView::LargeCanvas)) {
        flags_.setFlag(FullLayout, true);
        flags_.setFlag(CanSelect, false);
        flags_.setFlag(CanRotate, false);
    }
    transform_ = new ControlTransform(res->transform());
    if (res_->flags() & ResourceView::SavedSession) {
        flags_ |= RestoreSession;
        if (res_->flags().testFlag(ResourceView::LargeCanvas)) {
            flags_.setFlag(DefaultFlags, false);
        }
    }
}

Control::~Control()
{
    if (transform_)
        delete transform_;
    if (realItem_)
        delete realItem_;
    realItem_ = nullptr;
    item_ = nullptr;
    transform_ = nullptr;
    res_ = nullptr;
}

void Control::attachTo(QGraphicsItem * parent)
{
    item_ = create(res_);
    if (transform_)
        item_->setTransformations({transform_});
    item_->setData(ITEM_KEY_CONTROL, QVariant::fromValue(this));
    realItem_ = item_;
    if (flags_ & WithSelectBar) {
        itemFrame()->addTopBar();
    }
    attaching();
    realItem_->setParentItem(parent);
    loadSettings();
    sizeChanged();
    initPosition();
    relayout();
    attached();
    if (!(flags_ & LoadFinished) && !stateItem_) {
        stateItem()->setLoading(res_->name());
    }
}

void Control::detachFrom(QGraphicsItem *parent)
{
    detaching();
    saveSettings();
    (void) parent;
    realItem_->scene()->removeItem(realItem_);
    realItem_->setTransformations({});
    realItem_->setData(ITEM_KEY_CONTROL, QVariant());
    detached();
    //deleteLater();
    delete this;
}

void Control::relayout()
{
    if (flags_ & FullLayout) {
        resize(realItem_->parentItem()->boundingRect().size());
    } else {

    }
}

void Control::beforeClone()
{
    saveSettings();
}

void Control::attaching()
{
}

void Control::attached()
{
    loadFinished(true);
}

void Control::detaching()
{
}

void Control::detached()
{
}

void Control::loadSettings()
{
    for (QByteArray & k : res_->dynamicPropertyNames())
        setProperty(k, res_->property(k));
}

void Control::saveSettings()
{
    for (int i = Control::metaObject()->propertyCount();
            i < metaObject()->propertyCount(); ++i) {
        QMetaProperty p = metaObject()->property(i);
        res_->setProperty(p.name(), p.read(this));
    }
    // special one
    res_->setProperty("sizeHint", sizeHint());
    for (QByteArray & k : dynamicPropertyNames())
        res_->setProperty(k, property(k));
    res_->setSaved();
}

void Control::sizeChanged()
{
    QRectF rect = item_->boundingRect();
    QPointF center(rect.center());
    if (flags_ & LoadFinished) {
        if (flags_ & Adjusting) {
            item_->setTransform(QTransform::fromTranslate(-center.x(), -center.y()));
        } else {
            // keep top left
            QTransform t = item_->transform();
            center = t.map(center);
            move(center);
            t.translate(-center.x(), -center.y());
            item_->setTransform(t);
        }
        if (realItem_ != item_)
            static_cast<ItemFrame *>(realItem_)->updateRect();
    } else {
        item_->setTransform(QTransform::fromTranslate(-center.x(), -center.y()));
    }
    if (stateItem_) {
        stateItem_->updateTransform();
    }
    ItemSelector * selector = static_cast<WhiteCanvas *>(
                realItem_->parentItem()->parentItem())->selector();
    selector->updateSelect(realItem_);
}

QSizeF Control::sizeHint()
{
    return item_->boundingRect().size();
}

static void adjustSizeHint(QSizeF & size, QSizeF const & psize)
{
    if (size.width() < 10.0) {
        size.setWidth(psize.width() * size.width());
    }
    if (size.height() < 0)
        size.setHeight(size.width() * -size.height());
    else if (size.height() < 10.0)
        size.setHeight(psize.height() * size.height());
}

// called before attached
void Control::setSizeHint(QSizeF const & size)
{
    if (size.width() < 10.0 || size.height() < 10.0) {
        QSizeF size2 = size;
        QRectF rect = realItem_->parentItem()->boundingRect();
        adjustSizeHint(size2, rect.size());
        resize(size2);
    } else {
        resize(size);
    }
}

void Control::resize(QSizeF const & size)
{
    (void) size;
    sizeChanged();
}

static constexpr qreal CROSS_LENGTH = 20;

Control::SelectMode Control::selectTest(QPointF const & point)
{
    if (flags_ & FullSelect)
        return Select;
    if (res_->flags().testFlag(ResourceView::LargeCanvas))
        return PassSelect;
    if ((flags_ & HelpSelect) == 0)
        return NotSelect;
    QRectF rect = item_->boundingRect();
    rect.adjust(CROSS_LENGTH, CROSS_LENGTH, -CROSS_LENGTH, -CROSS_LENGTH);
    return rect.contains(point) ? NotSelect : Select;
}

static qreal polygonArea(QPolygonF const & p)
{
    qreal area = 0;
    int j = 0;
    for (int i = 1; i < p.size(); ++i) {
        area += (p[j].x() + p[i].x()) * (p[j].y() - p[i].y());
        j = i;
    }
    return qAbs(area) / 2.0;
}

void Control::initPosition()
{
    if (realItem_ != item_)
        static_cast<ItemFrame *>(realItem_)->updateRect();
    if (flags_ & (FullLayout | RestoreSession | PositionAtCenter))
        return;
    QGraphicsItem *parent = realItem_->parentItem();
    QPolygonF polygon;
    for (QGraphicsItem * c : parent->childItems()) {
        if (c == realItem_ || Control::fromItem(c)->flags() & FullLayout)
            continue;
        polygon = polygon.united(c->mapToParent(c->boundingRect()));
    }
    QRectF rect = parent->boundingRect();
    qreal dx = rect.width() / 3.0;
    qreal dy = rect.height() / 3.0;
    rect.adjust(0, 0, -dx - dx, -dy - dy);
    qreal minArea = dx * dy;
    QPointF pos;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            qreal area = polygonArea(polygon.intersected(rect));
            if (qFuzzyIsNull(minArea - area) && i == 1 && j == 1) {
                pos = rect.center();
            } else if (minArea > area) {
                minArea = area;
                pos = rect.center();
            }
            rect.adjust(dx, 0, dx, 0);
        }
        qreal dx3 = -dx * 3;
        rect.adjust(dx3, dy, dx3, dy);
    }
    res_->transform().translate(pos);
}

void Control::loadFinished(bool ok, QString const & iconOrMsg)
{
    if (ok) {
        if (iconOrMsg.isNull()) {
            if (stateItem_) {
                QList<QGraphicsTransform*> trs = stateItem_->transformations();
                stateItem_->setTransformations({});
                for (QGraphicsTransform* tr: trs)
                    delete tr;
                delete stateItem_;
                stateItem_ = nullptr;
            }
        } else {
            stateItem()->setLoaded(iconOrMsg);
        }
        sizeChanged();
        initScale();
        flags_ |= LoadFinished;
    } else {
        stateItem()->setFailed(iconOrMsg.isEmpty() ? "加载失败，点击即可重试" : iconOrMsg);
        QObject::connect(stateItem(), &StateItem::clicked, this, &Control::reload);
    }
}

void Control::initScale()
{
    if (realItem_ != item_)
        static_cast<ItemFrame *>(realItem_)->updateRect();
    QSizeF ps = realItem_->parentItem()->boundingRect().size();
    QSizeF size = item_->boundingRect().size();
    if (res_->flags().testFlag(ResourceView::LargeCanvas)) {
        Control * canvasControl = fromItem(
                    realItem_->parentItem()->parentItem());
        if (canvasControl) {
            canvasControl->flags_.setFlag(CanScale, flags_.testFlag(CanScale));
            flags_.setFlag(DefaultFlags, 0);
            if (flags_ & RestoreSession) {
                return;
            }
            canvasControl->resize(size);
            size -= ps;
            QPointF d(size.width(), size.height());
            canvasControl->move(d);
            return;
        }
    }
    /*
    if (flags_ & RestoreSession) {
        QVariant sizeHint = res_->property("sizeHint");
        if (sizeHint.isValid()) {
            QSizeF sh = sizeHint.toSizeF();
            if (sh.width() < 10.0 || sh.height() < 10.0) {
                adjustSizeHint(sh, ps);
            }
            resize(sh);
        }
        return;
    }*/
    if (flags_ & (FullLayout | LoadFinished)) {
        return;
    }
    if (item_ != realItem_) {
        QRectF padding(static_cast<ItemFrame *>(realItem_)->padding());
        ps.setWidth(ps.height() - padding.width());
        ps.setHeight(ps.height() - padding.height());
    }
    qreal scale = 1.0;
    while (size.width() > ps.width() || size.height() > ps.height()) {
        size /= 2.0;
        scale /= 2.0;
    }
    if (flags_ & ExpandScale) {
        while (size.width() * 2.0 < ps.width() && size.height() * 2.0 < ps.height()) {
            size *= 2.0;
            scale *= 2.0;
        }
    }
    if (flags_ & LayoutScale) {
        resize(size);
    } else {
        res_->transform().scaleTo(scale);
    }
    if (realItem_ != item_)
        static_cast<ItemFrame *>(realItem_)->updateRect();
}

void Control::setSize(const QSizeF &size)
{
    QSizeF size2(size);
    adjustSizeHint(size2, realItem_->parentItem()->boundingRect().size());
    if (flags_ & Adjusting) {
        setProperty("delayResize", size2);
    } else {
        resize(size2);
    }
}

void Control::move(QPointF & delta)
{
    res_->transform().translate(delta);
}

bool Control::scale(QRectF &rect, const QRectF &direction, QPointF &delta)
{
    QRectF padding;
    if (realItem_ != item_)
        padding = itemFrame()->padding();
    bool result = res_->transform().scale(rect, direction, delta, padding,
                            flags_ & KeepAspectRatio, flags_ & LayoutScale, 120.0);
    if (!result)
        return false;
    QRectF origin = rect;
    if (item_ != realItem_) {
        static_cast<ItemFrame *>(realItem_)->updateRectToChild(origin);
    }
    if (flags_ & LayoutScale) {
        resize(origin.size());
    }
    return true;
}

void Control::gesture(const QPointF &from1, const QPointF &from2, QPointF &to1, QPointF &to2)
{
    res_->transform().gesture(from1, from2, to1, to2,
                              flags_ & CanMove, flags_ & CanScale, flags_ & CanRotate);
}

void Control::rotate(QPointF const & from, QPointF & to)
{
    res_->transform().rotate(from, to);
}

void Control::rotate(QPointF const & center, QPointF const & from, QPointF &to)
{
    res_->transform().rotate(center, from, to);
}

QRectF Control::boundRect() const
{
    QRectF rect = realItem_->boundingRect();
    if (item_ == realItem_) {
        rect.moveCenter({0, 0});
        QTransform const & scale = res_->transform().scale();
        rect = QRectF(rect.x() * scale.m11(), rect.y() * scale.m22(),
                      rect.width() * scale.m11(), rect.height() * scale.m22());
    }
    return rect;
}

void Control::select(bool selected)
{
    flags_.setFlag(Selected, selected);
    if (realItem_ != item_)
        static_cast<ItemFrame *>(realItem_)->setSelected(selected);
}

void Control::adjusting(bool be)
{
    flags_.setFlag(Adjusting, be);
    if (!be) {
        QVariant delayResize = property("delayResize");
        if (delayResize.isValid()) {
            resize(delayResize.toSizeF());
            setProperty("delayResize", QVariant());
        }
    }
}

ItemFrame * Control::itemFrame()
{
    if (item_ != realItem_) {
        return static_cast<ItemFrame*>(realItem_);
    }
    ItemFrame * frame = new ItemFrame(item_);
    realItem_ = frame;
    ControlTransform* ct = static_cast<ControlTransform*>(transform_)->addFrameTransform();
    realItem_->setData(ITEM_KEY_CONTROL, QVariant::fromValue(this));
    realItem_->setTransformations({ct});
    return frame;
}

void Control::loadStream()
{
    res_->resource()->getStream().then([this, l = life()] (QSharedPointer<QIODevice> stream) {
        if (l.isNull()) return;
        onStream(stream.get());
        loadFinished(true);
    }).fail([this, l = life()](std::exception& e) {
        loadFinished(true, e.what());
    });
}

void Control::loadData()
{
    res_->resource()->getData().then([this, l = life()] (QByteArray data) {
        if (l.isNull()) return;
        onData(data);
        loadFinished(true);
    }).fail([this, l = life()](std::exception& e) {
        loadFinished(true, e.what());
    });
}

void Control::loadText()
{
    res_->resource()->getText().then([this, l = life()] (QString text) {
        if (l.isNull()) return;
        onText(text);
        loadFinished(true);
    }).fail([this, l = life()](std::exception& e) {
        loadFinished(true, e.what());
    });
}

void Control::reload()
{
    QObject::disconnect(stateItem(), &StateItem::clicked, this, &Control::reload);
    if (!(flags_ & LoadFinished)) {
        stateItem()->setLoading(res_->name());
        attached(); // reload
    }
}

void Control::onStream(QIODevice *stream)
{
    (void) stream;
    throw std::exception("Not implemets onStream");
}

void Control::onData(QByteArray data)
{
    (void) data;
    throw std::exception("Not implemets onData");
}

void Control::onText(QString text)
{
    (void) text;
    throw std::exception("Not implemets onText");
}

void Control::getToolButtons(QList<ToolButton *> &buttons, const QList<ToolButton *> &parents)
{
    ToolButtonProvider::getToolButtons(buttons, parents);
    if (parents.isEmpty()) {
        btnFastCopy.flags.setFlag(ToolButton::Checked, false);
        if (!buttons.empty())
            buttons.append(&ToolButton::SPLITER);
        if (res_->canMoveTop())
            buttons.append(&btnTop);
        if (res_->flags() & ResourceView::CanCopy)
            buttons.append(&btnCopy);
        if (res_->flags() & ResourceView::CanCopy)
            buttons.append(&btnFastCopy);
        if (res_->flags() & ResourceView::CanDelete)
            buttons.append(&btnDelete);
        if (buttons.endsWith(&ToolButton::SPLITER))
            buttons.pop_back();
    }
}

void Control::setOption(QString const & key, QVariant value)
{
    ToolButtonProvider::setOption(key, value);
    res_->setProperty(key.toUtf8(), value);
}

StateItem * Control::stateItem()
{
    if (stateItem_)
        return stateItem_;
    stateItem_ = new StateItem(item_);
    stateItem_->setData(ITEM_KEY_CONTROL, QVariant::fromValue(this));
    ControlTransform * ct = new ControlTransform(static_cast<ControlTransform*>(transform_), true, false, false);
    stateItem_->setTransformations({ct});
    return stateItem_;
}

