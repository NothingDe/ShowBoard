#include "resourcemanager.h"
#include "resource.h"
#include "resourcefactory.h"
#include "resourceview.h"
#include "qlazy.hpp"
#include "qcomponentcontainer.h"

#include "resources/resources.h"
#include "showboard.h"

ResourceManager * ResourceManager::instance()
{
    static ResourceManager * manager = nullptr;
    if (manager == nullptr)
        manager = ShowBoard::containter().get_export_value<ResourceManager>();
    return manager;
}

static QExport<ResourceManager> export_(QPart::shared);
static QImportMany<ResourceManager, ResourceView> import_resources("resource_types", QPart::nonshared, true);

ResourceManager::ResourceManager(QObject *parent)
    : QObject(parent)
{
    //Q_INIT_RESOURCE(ShowBoard);
}

void ResourceManager::onComposition()
{
    for (auto & r : resource_types_) {
        QString types = r.part()->attr(ResourceView::EXPORT_ATTR_TYPE);
        for (auto t : types.split(",", QString::SkipEmptyParts)) {
            resources_[t] = &r;
        }
    }
}

void ResourceManager::mapResourceType(QString const & from, QString const & to)
{
    mapTypes_[from] = to;
}

static constexpr char DATA_SCHEME_SEP[] = { ';', ',' };

ResourceView * ResourceManager::createResource(QUrl const & uri)
{
    std::map<QString, QLazy*>::iterator iter = resources_.end();
    QString originType = "";
    QString type = "";
    if (uri.scheme() == "data")
    {
        int n = uri.path().indexOf(DATA_SCHEME_SEP);
        originType = uri.path().left(n);
        type = mapTypes_.value(originType, originType);
        iter = resources_.find(type);
    }
    else
    {
        originType = uri.scheme();
        type = mapTypes_.value(originType, originType);
        iter = resources_.find(type);
        if (iter == resources_.end())
        {
            int n = uri.path().lastIndexOf('.');
            if (n > 0) {
                originType = uri.path().mid(n + 1);
                type = mapTypes_.value(originType, originType);
                iter = resources_.find(type);
            }
        }
    }
    ResourceView* rv = nullptr;
    if (iter == resources_.end()) {
        if (!type.isEmpty())
            rv = new ResourceView(type, uri);
    } else {
        Resource * res = new Resource(iter->first, uri);
        char const * rfactory = iter->second->part()->attr(ResourceView::EXPORT_ATTR_FACTORY);
        if (rfactory && strcmp(rfactory, "true") == 0) {
            ResourceFactory * factory = iter->second->get<ResourceFactory>();
            return factory->create(res);
        }
        rv = iter->second->create<ResourceView>(Q_ARG(Resource*, res));
    }
    if (rv) {
        rv->resource()->setProperty(Resource::PROP_ORIGIN_TYPE, originType);
    }
    return rv;
}

ResourceFactory * ResourceManager::getFactory(QString const & type)
{
    std::map<QString, QLazy*>::iterator iter = resources_.find(type);
    if (iter == resources_.end())
        return nullptr;
    char const * rfactory = iter->second->part()->attr(ResourceView::EXPORT_ATTR_FACTORY);
    if (rfactory && strcmp(rfactory, "true") == 0) {
        return iter->second->get<ResourceFactory>();
    } else {
        return nullptr;
    }
}
