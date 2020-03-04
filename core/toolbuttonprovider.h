#ifndef TOOLBUTTONPROVIDER_H
#define TOOLBUTTONPROVIDER_H

#include "ShowBoard_global.h"
#include "lifeobject.h"

#include <QObject>
#include <QMap>

class ToolButton;
class OptionToolButtons;

class SHOWBOARD_EXPORT ToolButtonProvider : public LifeObject
{
    Q_OBJECT
public:
    ToolButtonProvider(QObject * parent = nullptr);

    virtual ~ToolButtonProvider() override;

signals:
    void buttonsChanged();

public:
    /*
     * collect context menu of this control
     *  copy, delete is add according to flags
     *  other menus can be defined with toolsString()
     */
    virtual void getToolButtons(QList<ToolButton *> & buttons,
                                QList<ToolButton *> const & parents = {});

    virtual void getToolButtons(QList<ToolButton *> & buttons,
                                ToolButton * parent);

    /*
     * handle button click,
     *  copy, delete are handled by canvas and should not go here
     */
    virtual void handleToolButton(QList<ToolButton *> const & buttons);

    virtual void handleToolButton(ToolButton * button, QStringList const & args);

    virtual void updateToolButton(ToolButton * button);

public:
    /*
     * invoke slot by name, use for lose relation call
     */
    void exec(QByteArray const & cmd, QGenericArgument arg0 = QGenericArgument(),
              QGenericArgument arg1 = QGenericArgument(), QGenericArgument arg2 = QGenericArgument());

    /*
     * invoke slot by name, use for lose relation call
     */
    void exec(QByteArray const & cmd, QStringList const & args);

public:
    virtual void setOption(QByteArray const & key, QVariant value);

    virtual QVariant getOption(QByteArray const & key);

protected:
    void setToolsString(QString const & tools);

    /*
     * stringlized definition of context menus
     *   menu strings is seperated with ';' and menu define parts with '|'
     *   for example:
     *     "open()|打开|:/showboard/icons/icon_open.png;"
     */
    virtual QString toolsString(QByteArray const & parent = nullptr) const;

    QList<ToolButton *> tools(QByteArray const & parent = nullptr);

private:
    QMap<ToolButton*, ToolButton*> nonSharedButtons_;
    QMap<QString, OptionToolButtons*> optionButtons_;
};

class SHOWBOARD_EXPORT RegisterOptionsButtons
{
public:
    RegisterOptionsButtons(QMetaObject const & meta, char const * parent, OptionToolButtons & buttons);
};

#define REGISTER_OPTION_BUTTONS(type, parent, buttons) \
    static RegisterOptionsButtons export_option_buttons_##parent(type::staticMetaObject, #parent, buttons);

#endif // TOOLBUTTONPROVIDER_H
