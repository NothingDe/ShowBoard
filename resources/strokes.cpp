#include "strokes.h"
#include "core/resource.h"

using namespace QtPromise;

Strokes::Strokes(Resource * res, Flags flags, Flags clearFlags)
    : ResourceView(res, flags, clearFlags)
{
}

Strokes::Strokes(Strokes const & o)
    : ResourceView(o)
{
}

QPromise<StrokesReader*> Strokes::load()
{
    if (url().scheme() == res_->type()) // should not happen
        return QPromise<StrokesReader*>::resolve(nullptr);
    stream_.reset();
    int n = url().path().lastIndexOf('.');
    if (n < 0)
        throw std::invalid_argument("no stroke type");
    qDebug() << "Strokes::load";
    QByteArray type = url().path().mid(n + 1).toUtf8();
    return res_->getStream().then([this, l = life(), type](QSharedPointer<QIODevice> stream) {
        if (l.isNull())
            throw std::runtime_error("life dead");
        StrokesReader * reader = StrokesReader::createReader(stream.get(), type);
        if (!reader)
            throw std::runtime_error("StrokeReader not found");
        stream_ = stream;
        return reader;
    });
}
