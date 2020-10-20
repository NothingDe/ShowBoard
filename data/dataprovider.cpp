#include "dataprovider.h"
#include "dataurlcodec.h"
#include "resourcecache.h"
#include <showboard.h>

#include <qexport.h>
#include <qlazy.h>
#include <qcomponentcontainer.h>
#include <QBuffer>

#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <qeventbus.h>

#include <core/resource.h>

using namespace QtPromise;

REGISTER_DATA_RPOVIDER(DataDataProvider,"data")
REGISTER_DATA_RPOVIDER(FileDataProvider,"file,qrc,")
REGISTER_DATA_RPOVIDER(HttpDataProvider,"http,https")

DataProvider *DataProvider::getProvider(const QByteArray &scheme)
{
    static QVector<QLazy> types;
    static QMap<QByteArray, QLazy*> readerTypes;
    if (readerTypes.empty()) {
         types = ShowBoard::containter().getExports<DataProvider>(QPart::nonshared);
         for (auto & r : types) {
             QByteArray types = r.part()->attrMineType();
             for (auto t : types.split(',')) {
                 readerTypes[t.trimmed()] = &r;
             }
         }
    }
    auto iter = readerTypes.find(scheme);
    if (iter == readerTypes.end())
        return nullptr;
    return iter.value()->get<DataProvider>();
}

DataProvider::DataProvider(QObject *parent)
    : QObject(parent)
{
}

/* DataDataProvider */

DataDataProvider::DataDataProvider(QObject *parent)
    : DataProvider(parent)
{
}

QtPromise::QPromise<QSharedPointer<QIODevice> > DataDataProvider::getStream(QObject * context, const QUrl &url, bool)
{
    auto data = DataUrlCodec::decodeDataUrl(url.toEncoded());
    QBuffer * buf(new QBuffer);
    buf->setData(data.data);
    buf->open(QIODevice::ReadOnly);
    if (context) {
        if (!data.mimeTypeName.isEmpty())
            context->setProperty("mimeType", data.mimeTypeName);
        if (!data.charset.isEmpty())
            context->setProperty("charset", data.charset);
    }
    return QPromise<QSharedPointer<QIODevice>>::resolve(QSharedPointer<QIODevice>(buf));
}

/* FileDataProvider */

FileDataProvider::FileDataProvider(QObject *parent)
    : DataProvider(parent)
{
}

QtPromise::QPromise<QSharedPointer<QIODevice> > FileDataProvider::getStream(QObject *, const QUrl &url, bool all)
{
    (void) all;
    QString path = url.scheme() == "qrc" ? ":" + url.path() : url.toLocalFile();
    QSharedPointer<QIODevice> file(new QFile(path));
    if (file->open(QFile::ReadOnly | QFile::ExistingOnly)) {
        return QPromise<QSharedPointer<QIODevice>>::resolve(file);
    } else {
        qDebug() << "FileDataProvider:" << file->errorString();
        return QPromise<QSharedPointer<QIODevice>>::reject(std::invalid_argument("打开失败，请重试"));
    }
}

/* HttpDataProvider */

HttpDataProvider::HttpDataProvider(QObject *parent)
    : DataProvider(parent)
{
    network_ = new QNetworkAccessManager();
    network_->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

QtPromise::QPromise<QSharedPointer<QIODevice>> HttpDataProvider::getStream(QObject * context, const QUrl &url, bool all)
{
    QNetworkRequest request(url);
    QSharedPointer<HttpStream> reply(new HttpStream(context, network_->get(request)));
    reply->setObjectName(url.toString());
    return QPromise<QSharedPointer<QIODevice>>([reply, all](
                                     const QPromiseResolve<QSharedPointer<QIODevice>>& resolve,
                                     const QPromiseReject<QSharedPointer<QIODevice>>& reject) {

        auto error = [reply, reject](QNetworkReply::NetworkError) {
            reject(std::invalid_argument("network|打开失败，请检查网络再试"));
        };
        QObject::connect(reply.get(), &HttpStream::error, error);
        if (all) {
            auto finished = [reply, resolve, error]() {
                resolve(reply);
            };
            QObject::connect(reply.get(), &HttpStream::finished, finished);
        } else {
            auto readyRead = [reply, resolve]() {
                 resolve(reply);
            };
            QObject::connect(reply.get(), &HttpStream::readyRead, readyRead);
        }
    }).finally([reply] () {
        reply->disconnect();
    });
}

HttpStream::HttpStream(QObject * context, QNetworkReply *reply)
    : reply_(reply)
    , paused_(nullptr)
    , aborted_(false)
    , lastPos_(0)
    , speed_(0)
    , elapsed_(0)
{
    open(ReadOnly);
    if (context) {
        QObject::connect(context, &QObject::destroyed,
                         this, &HttpStream::abort);
        if (ResourceCacheLife * life = qobject_cast<ResourceCacheLife*>(context)) {
            QObject:: connect(life, &ResourceCacheLife::pause,
                              this, &HttpStream::pause);
            QObject:: connect(life, &ResourceCacheLife::resume,
                              this, &HttpStream::resume);
        }
        setProperty("context", QVariant::fromValue(context));
    }
    reopen();
    if (qobject_cast<Resource*>(context))
        startTimer(1000);
}

HttpStream::~HttpStream()
{
    qDebug() << "HttpStream destroyed" << this;
    reply_->deleteLater();
    if (elapsed_)
        ResourceCache::resume(this);
}

bool HttpStream::connect(QIODevice * stream, std::function<void ()> finished,
                         std::function<void (std::exception &&)> error)
{
    QNetworkReply * reply = qobject_cast<QNetworkReply*>(stream);
    if (reply == nullptr) {
        if (HttpStream * http = qobject_cast<HttpStream*>(stream))
            reply = http->reply_;
    }
    if (reply) {
        auto error2 = [error](QNetworkReply::NetworkError e) {
            qDebug() << "HttpStream:" << e;
            error(std::invalid_argument("network|打开失败，请检查网络再试"));
        };
        if (reply->isFinished()) {
            if (reply->error())
                error2(reply->error());
            else
                finished();
            return true;
        }
        if (HttpStream * http = qobject_cast<HttpStream*>(stream)) {
            QObject::connect(http, &HttpStream::error, error2);
        } else {
            void (QNetworkReply::*p)(QNetworkReply::NetworkError) = &QNetworkReply::error;
            QObject::connect(reply, p, error2);
        }
    }
    return false;
}

void HttpStream::onError(QNetworkReply::NetworkError e)
{
    qDebug() << "HttpStream onError" << e << this;
    if (paused_)
        return;
    if (!aborted_ && (e <= QNetworkReply::UnknownNetworkError
            || e >= QNetworkReply::ProtocolUnknownError)) {
        qint64 size = pos();
        QNetworkRequest request = reply_->request();
        qDebug() << "HttpStream retry" << e << size;
        if (size > 0)
            request.setRawHeader("Range", "bytes=" + QByteArray::number(size) + "-");
        QNetworkReply * reply = reply_->manager()->get(request);
        std::swap(reply, reply_);
        //delete reply;
        reopen();
        return;
    }
    setErrorString(reply_->errorString());
    emit error(e);
}

void HttpStream::onReadyRead()
{
    if (auto context = property("context").value<QObject*>()) {
        QVariant ct = reply_->header(QNetworkRequest::ContentTypeHeader);
        if (ct.isValid()) {
            auto format = DataUrlCodec::decodeDataUrl(
                        QByteArray("data:") + ct.toByteArray() + ",");
            if (!format.mimeTypeName.isEmpty())
                context->setProperty("mimeType", format.mimeTypeName);
            if (!format.charset.isEmpty())
                context->setProperty("charset", format.charset);
        }
        setProperty("context", QVariant());
    }
}

void HttpStream::onFinished()
{
    qDebug() << "HttpStream onFinished" << this;
    if (sender() == reply_) {
        qDebug() << "HttpStream onFinished" << pos();
        emit readChannelFinished();
        emit finished();
    } else if (sender() == paused_) {
    } else {
        // we are retrying
        sender()->deleteLater();
    }
}

void HttpStream::reopen()
{
    QObject::connect(reply_, &QNetworkReply::finished, this, &HttpStream::onFinished);
    QObject::connect(reply_, &QNetworkReply::readyRead, this, &HttpStream::onReadyRead);
    void (QNetworkReply::*p)(QNetworkReply::NetworkError) = &QNetworkReply::error;
    QObject::connect(reply_, p, this, &HttpStream::onError);
    //pos_ = 0;
}

void HttpStream::pause()
{
    if (paused_)
        return;
    qDebug() << "HttpStream pause" << this;
    std::swap(paused_, reply_);
    paused_->abort();
}

void HttpStream::resume()
{
    if (!paused_)
        return;
    qDebug() << "HttpStream resume" << this;
    std::swap(paused_, reply_);
    onError(reply_->error());
}

void HttpStream::abort()
{
    if (aborted_)
        return;
    qDebug() << "HttpStream abort" << this;
    aborted_ = true;
    if (paused_) {
        std::swap(reply_, paused_);
        emit error(QNetworkReply::OperationCanceledError);
        emit readChannelFinished();
        emit finished();
        return;
    }
    reply_->abort();
}

qint64 HttpStream::readData(char *data, qint64 maxlen)
{
    if (!reply_ || !reply_->isOpen()) // maybe paused
        return 0;
    qint64 result = reply_->read(data, maxlen);
    /*
    if (result) {
        if (pos_ == 0) {
            QByteArray msg;
            for (auto h : reply_->rawHeaderPairs())
                msg.append(h.first + ':' + h.second + '\n');
            qDebug() << msg;
        }
        pos_ += result;
        if (pos_ > 10000000) {
            QTimer::singleShot(0, reply_, &QNetworkReply::abort);
        }
    }
    */
    return result;
}

qint64 HttpStream::writeData(const char *, qint64)
{
    assert(false);
    return 0;
}

void HttpStream::timerEvent(QTimerEvent *)
{
    qint64 pos = this->pos() + reply_->bytesAvailable();
    qint64 diff = pos - lastPos_;
    speed_ = speed_ /2 + diff;
    lastPos_ = pos;
    ++elapsed_;
    bool stop = false;
    if (speed_ < 100) {
        if (elapsed_ == 1) {
            ResourceCache::pause(this);
        } else if (elapsed_ == 10) {
            stop = true;
        }
    }
    if (speed_ < 10000) {
        if (elapsed_ == 5) {
            ResourceCache::pause(this);
            QEventBus::globalInstance().publish("warning", "当前网络较慢，请耐心等待");
        } else if (elapsed_ == 60) {
            stop = true;
        }
    }
    // no data elapsed 10
    // low speed elapsed 60
    if (stop) {
        qDebug() << "HttpStream stop" << this;
        pause();
        std::swap(reply_, paused_);
        emit error(QNetworkReply::TimeoutError);
        emit readChannelFinished();
        emit finished();
    }
}
