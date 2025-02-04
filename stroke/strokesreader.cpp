﻿#include "strokesreader.h"
#include "showboard.h"

#include <qcomponentcontainer.h>
#include <qlazy.hpp>

#include <QIODevice>
#include <QMetaClassInfo>

StrokesReader *StrokesReader::createReader(QIODevice *stream, const QByteArray &format)
{
    static QVector<QLazy> types;
    static QMap<QByteArray, QLazy*> readerTypes;
    if (readerTypes.empty()) {
         types = ShowBoard::containter().getExports<StrokesReader>(QPart::nonshared);
         for (auto & r : types) {
             QByteArray types = r.part()->attrMineType();
             for (auto t : types.split(',')) {
                 readerTypes[t.trimmed()] = &r;
             }
         }
    }
    auto iter = readerTypes.find(format);
    if (iter == readerTypes.end())
        return nullptr;
    StrokesReader * p = (*iter)->create<StrokesReader>(Q_ARG(QIODevice*,stream));
    p->setProperty(QPart::ATTR_MINE_TYPE, format);
    return p;
}

StrokesReader::StrokesReader(QIODevice * stream, QObject *parent)
    : QObject(parent)
    , stream_(stream)
{
}

StrokesReader::~StrokesReader()
{
    close();
    if (!stream2_)
        delete stream_;
}

bool StrokesReader::getMaximun(StrokePoint &point)
{
    int pos = 0;
    return read(point, pos);
}

bool StrokesReader::seek(int bytePos)
{
    return stream_->seek(bytePos);
}

int StrokesReader::bytePos()
{
    return static_cast<int>(stream_->pos());
}

bool StrokesReader::startAsyncRead(StrokesReader::AsyncHandler handler)
{
    if (stream_->atEnd())
        return false;
    connect(stream_, &QIODevice::readyRead, this, [this, handler] () {
        StrokePoint point;
        int pos = 0;
        while (read(point, pos))
            handler(point, pos);
    });
    connect(stream_, &QIODevice::readChannelFinished, this, [this] () {
        emit asyncFinished();
    });
    return true;
}

void StrokesReader::stopAsyncRead()
{
    stream_->disconnect(this);
}

void StrokesReader::close()
{
    stream_->close();
}

void StrokesReader::storeStreamLife(QSharedPointer<QIODevice> stream)
{
    assert(stream.get() == stream_);
    stream2_ = stream;
}
