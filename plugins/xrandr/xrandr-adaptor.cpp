/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp usd-xrandr-manager.xml -a xrandr-adaptor
 *
 * qdbusxml2cpp is Copyright (C) 2020 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#include "xrandr-adaptor.h"
#include <QtCore/QMetaObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

/*
 * Implementation of adaptor class XrandrAdaptor
 */

XrandrAdaptor::XrandrAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    // constructor
    setAutoRelaySignals(true);
}

XrandrAdaptor::~XrandrAdaptor()
{
    // destructor
}

int XrandrAdaptor::height()
{
    // handle method call org.ukui.SettingsDaemon.xrandr.height
    int out0;
    QMetaObject::invokeMethod(parent(), "height", Q_RETURN_ARG(int, out0));
    return out0;
}

int XrandrAdaptor::priScreenChanged(int x, int y, int width, int height)
{
    // handle method call org.ukui.SettingsDaemon.xrandr.priScreenChanged
    int out0;
    QMetaObject::invokeMethod(parent(), "priScreenChanged", Q_RETURN_ARG(int, out0), Q_ARG(int, x), Q_ARG(int, y), Q_ARG(int, width), Q_ARG(int, height));
    return out0;
}

int XrandrAdaptor::width()
{
    // handle method call org.ukui.SettingsDaemon.xrandr.width
    int out0;
    QMetaObject::invokeMethod(parent(), "width", Q_RETURN_ARG(int, out0));
    return out0;
}

int XrandrAdaptor::x()
{
    // handle method call org.ukui.SettingsDaemon.xrandr.x
    int out0;
    QMetaObject::invokeMethod(parent(), "x", Q_RETURN_ARG(int, out0));
    return out0;
}

int XrandrAdaptor::y()
{
    // handle method call org.ukui.SettingsDaemon.xrandr.y
    int out0;
    QMetaObject::invokeMethod(parent(), "y", Q_RETURN_ARG(int, out0));
    return out0;
}

