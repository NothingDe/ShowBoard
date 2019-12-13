#include "resourcepackage.h"
#include "resourcepage.h"
#include "qcomponentcontainer.h"
#include "resourcemanager.h"

extern QComponentContainer & ShowBoard_containter();


ResourcePackage::ResourcePackage(QObject *parent)
    : QObject(parent)
    , current_(-1)
{
    globalPage_ = new ResourcePage(this);
    newPage();
}

ResourcePage *ResourcePackage::globalPage()
{
    return globalPage_;
}

ResourcePage * ResourcePackage::newPage()
{
    ResourcePage * page = new ResourcePage(this);
    addPage(page);
    emit currentPageChanged(page);
    return page;
}

ResourcePage * ResourcePackage::currentPage()
{
    return pages_[current_];
}

int ResourcePackage::currentNumber()
{
    return current_ + 1;
}

void ResourcePackage::gotoFront()
{
    current_ = 0;
    emit currentPageChanged(pages_[current_]);
}

void ResourcePackage::gotoNext()
{
    if (++current_ < pages_.size())
        emit currentPageChanged(pages_[current_]);
    else
        --current_;
}

void ResourcePackage::gotoPrevious()
{
    if (--current_ >= 0)
        emit currentPageChanged(pages_[current_]);
    else
        ++current_;
}

void ResourcePackage::gotoBack()
{
    current_ = pages_.size() - 1;
    emit currentPageChanged(pages_[current_]);
}

void ResourcePackage::switchPage(int page)
{
    current_ = page;
    emit currentPageChanged(pages_[current_]);
}

void ResourcePackage::switchPage(ResourcePage * page)
{
    int n = pages_.indexOf(page);
    if (n < 0 || n == current_)
        return;
    switchPage(n);
}

void ResourcePackage::addPage(ResourcePage * page)
{
    page->setParent(this);
    ++current_;
    pages_.insert(current_, page);
}
