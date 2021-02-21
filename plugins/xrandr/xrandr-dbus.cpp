#include "xrandr-dbus.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDebug>
#define DBUS_NAME  "org.ukui.SettingsDaemon"
#define DBUS_PATH  "/org/ukui/SettingsDaemon/wayland"
#define DBUS_INTER "org.ukui.SettingsDaemon.wayland"

xrandrDbus::xrandrDbus(QObject* parent) :
    QObject(parent){

}

int xrandrDbus::x() {
    return mX;
}

int xrandrDbus::y() {
    return mY;
}

int xrandrDbus::width() {
    return mWidth;
}

int xrandrDbus::height() {
    return mHeight;
}

QString xrandrDbus::priScreenName(){
    return mName;
}

int xrandrDbus::priScreenChanged(int x, int y, int width, int height, QString name)
{
    mX = x;
    mY = y;
    mWidth = width;
    mHeight = height;
    mName = name;
    //qDebug()<< x<< y << width << height;
    Q_EMIT screenPrimaryChanged(x, y, width, height);
    return 0;
}