#include "dataprovider.h"

#include <qexport.h>

#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>

using namespace QtPromise;

REGISTER_DATA_RPOVIDER(DataDataProvider,"data")
REGISTER_DATA_RPOVIDER(FileDataProvider,"file,qrc,")
REGISTER_DATA_RPOVIDER(HttpDataProvider,"http,https")

DataProvider::DataProvider(QObject *parent)
    : QObject(parent)
{
}

/* DataDataProvider */

DataDataProvider::DataDataProvider(QObject *parent)
    : DataProvider(parent)
{
}

QtPromise::QPromise<QSharedPointer<QIODevice> > DataDataProvider::getStream(const QUrl &url, bool all)
{
    (void) url;
    (void) all;
    return QPromise<QSharedPointer<QIODevice>>::resolve(nullptr);
}

/* FileDataProvider */

FileDataProvider::FileDataProvider(QObject *parent)
    : DataProvider(parent)
{
}

QtPromise::QPromise<QSharedPointer<QIODevice> > FileDataProvider::getStream(const QUrl &url, bool all)
{
    (void) all;
    QString path = url.scheme() == "qrc" ? ":" + url.path() : url.toLocalFile();
    QSharedPointer<QIODevice> file(new QFile(path));
    if (file->open(QFile::ReadOnly | QFile::ExistingOnly)) {
        return QPromise<QSharedPointer<QIODevice>>::resolve(file);
    } else {
        qDebug() << "Resource file error" << file->errorString();
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

QtPromise::QPromise<QSharedPointer<QIODevice>> HttpDataProvider::getStream(const QUrl &url, bool all)
{
    QNetworkRequest request(url);
    QSharedPointer<QNetworkReply> reply(network_->get(request));
    return QPromise<QSharedPointer<QIODevice>>([reply, all](
                                     const QPromiseResolve<QSharedPointer<QIODevice>>& resolve,
                                     const QPromiseReject<QSharedPointer<QIODevice>>& reject) {
        auto error = [reply, reject](QNetworkReply::NetworkError e) {
            qDebug() << "Resource NetworkError " << e << reply->errorString();
            reject(std::invalid_argument("network|打开失败，请检查网络再试"));
        };
        if (all) {
            auto finished = [reply, resolve, error]() {
                if (reply->error()) {
                    error(reply->error());
                } else {
                    resolve(reply);
                }
            };
            if (reply->isFinished()) {
                finished();
                return;
            }
            QObject::connect(reply.get(), &QNetworkReply::finished, finished);
        } else {
            auto readyRead = [reply, resolve]() {
                resolve(reply);
            };
            char c;
            if (reply->peek(&c, 1) > 0) {
                readyRead();
                return;
            }
            QObject::connect(reply.get(), &QNetworkReply::readyRead, readyRead);
        }
        void (QNetworkReply::*p)(QNetworkReply::NetworkError) = &QNetworkReply::error;
        QObject::connect(reply.get(), p, error);
    });
}
