/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp usd-xrandr-manager.xml -a xrandr-adaptor
 *
 * qdbusxml2cpp is Copyright (C) 2020 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * This file may have been hand-edited. Look for HAND-EDIT comments
 * before re-generating it.
 */

#ifndef XRANDR_ADAPTOR_H
#define XRANDR_ADAPTOR_H

#include <QtCore/QObject>
#include <QtDBus/QtDBus>
QT_BEGIN_NAMESPACE
class QByteArray;
template<class T> class QList;
template<class Key, class Value> class QMap;
class QString;
class QStringList;
class QVariant;
QT_END_NAMESPACE

/*
 * Adaptor class for interface org.ukui.SettingsDaemon.wayland
 */
class WaylandAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.ukui.SettingsDaemon.wayland")
    Q_CLASSINFO("D-Bus Introspection", ""
                                       "  <interface name=\"org.ukui.SettingsDaemon.wayland\">\n"
                                       "    <signal name=\"screenPrimaryChanged\">\n"
                                       "      <arg direction=\"out\" type=\"i\" name=\"x\"/>\n"
                                       "      <arg direction=\"out\" type=\"i\" name=\"y\"/>\n"
                                       "      <arg direction=\"out\" type=\"i\" name=\"width\"/>\n"
                                       "      <arg direction=\"out\" type=\"i\" name=\"height\"/>\n"
                                       "    </signal>\n"
                                       "    <signal name=\"brightnessDown\"/>\n"
                                       "    <signal name=\"brightnessUp\"/>\n"
                                       "    <method name=\"x\">\n"
                                       "      <arg direction=\"out\" type=\"i\"/>\n"
                                       "    </method>\n"
                                       "    <method name=\"y\">\n"
                                       "      <arg direction=\"out\" type=\"i\"/>\n"
                                       "    </method>\n"
                                       "    <method name=\"width\">\n"
                                       "      <arg direction=\"out\" type=\"i\"/>\n"
                                       "    </method>\n"
                                       "    <method name=\"height\">\n"
                                       "      <arg direction=\"out\" type=\"i\"/>\n"
                                       "    </method>\n"
                                       "    <method name=\"scale\">\n"
                                       "      <arg direction=\"out\" type=\"d\"/>\n"
                                       "    </method>\n"
                                       "    <method name=\"priScreenName\">\n"
                                       "      <arg direction=\"out\" type=\"s\"/>\n"
                                       "    </method>\n"
                                       "    <method name=\"priScreenChanged\">\n"
                                       "      <arg direction=\"out\" type=\"i\"/>\n"
                                       "      <arg direction=\"in\" type=\"i\" name=\"x\"/>\n"
                                       "      <arg direction=\"in\" type=\"i\" name=\"y\"/>\n"
                                       "      <arg direction=\"in\" type=\"i\" name=\"width\"/>\n"
                                       "      <arg direction=\"in\" type=\"i\" name=\"height\"/>\n"
                                       "      <arg direction=\"in\" type=\"s\" name=\"name\"/>\n"
                                       "    </method>\n"
                                       "    <method name=\"setScreenMode\">\n"
                                       "      <arg direction=\"out\" type=\"i\"/>\n"
                                       "      <arg direction=\"in\" type=\"s\" name=\"modeName\"/>\n"
                                       "      <arg direction=\"in\" type=\"s\" name=\"appName\"/>\n"
                                       "    </method>\n"
                                       "    <method name=\"getScreenMode\">\n"
                                       "      <arg direction=\"out\" type=\"i\"/>\n"
                                       "      <arg direction=\"in\" type=\"s\" name=\"appName\"/>\n"
                                       "    </method>\n"
                                       "  </interface>\n"
                                       "")
public:
    WaylandAdaptor(QObject *parent);
    virtual ~WaylandAdaptor();

public: // PROPERTIES
public Q_SLOTS: // METHODS
    int height();
    int priScreenChanged(int x, int y, int width, int height, const QString &name);
    int setScreenMode(const QString &modeName, const QString &appName);
    int getScreenMode(const QString &appName);
    QString priScreenName();
    double scale();
    int width();
    int x();
    int y();
Q_SIGNALS: // SIGNALS
    void brightnessDown();
    void brightnessUp();
    void screenPrimaryChanged(int x, int y, int width, int height);
};

#endif
