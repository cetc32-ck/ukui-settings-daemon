/* -*- Mode: C++; indent-tabs-mode: nil; tab-width: 4 -*-
 * -*- coding: utf-8 -*-
 *
 * Copyright (C) 2012 by Alejandro Fiestas Olivares <afiestas@kde.org>
 * Copyright 2016 by Sebastian Kügler <sebas@kde.org>
 * Copyright (c) 2018 Kai Uwe Broulik <kde@broulik.de>
 *                    Work sponsored by the LiMux project of
 *                    the city of Munich.
 * Copyright (C) 2020 KylinSoft Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QProcess>
#include <QX11Info>
#include "xrandr-manager.h"

#include <QOrientationReading>
#include <memory>

#include <KF5/KScreen/kscreen/config.h>
#include <KF5/KScreen/kscreen/log.h>
#include <KF5/KScreen/kscreen/output.h>
#include <KF5/KScreen/kscreen/edid.h>
#include <KF5/KScreen/kscreen/configmonitor.h>
#include <KF5/KScreen/kscreen/getconfigoperation.h>
#include <KF5/KScreen/kscreen/setconfigoperation.h>

extern "C"{
#include <glib.h>
//#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/Xrandr.h>
#include <xorg/xserver-properties.h>
#include <gudev/gudev.h>
#include "clib-syslog.h"
}

#define SETTINGS_XRANDR_SCHEMAS     "org.ukui.SettingsDaemon.plugins.xrandr"
#define XRANDR_ROTATION_KEY         "xrandr-rotations"
#define XRANDR_PRI_KEY              "primary"
#define XSETTINGS_SCHEMA            "org.ukui.SettingsDaemon.plugins.xsettings"
#define XSETTINGS_KEY_SCALING       "scaling-factor"

#define MAX_SIZE_MATCH_DIFF         0.05

#define DBUS_NAME  "org.ukui.SettingsDaemon"
#define DBUS_PATH  "/org/ukui/SettingsDaemon/wayland"
#define DBUS_INTER "org.ukui.SettingsDaemon.wayland"


typedef struct
{
    unsigned char *input_node;
    XIDeviceInfo dev_info;

}TsInfo;

XrandrManager::XrandrManager()
{
    QGSettings *mXsettings = new QGSettings(XSETTINGS_SCHEMA);
    mScale = mXsettings->get(XSETTINGS_KEY_SCALING).toDouble();
    if(mXsettings)
        delete mXsettings;

    KScreen::Log::instance();


    mDbus = new xrandrDbus();
    new WaylandAdaptor(mDbus);

    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if(sessionBus.registerService(DBUS_NAME)){
        sessionBus.registerObject(DBUS_PATH,
                                  mDbus,
                                  QDBusConnection::ExportAllContents);
    }

    mAcitveTime = new QTimer(this);

}

void XrandrManager::getInitialConfig()
{
    connect(new KScreen::GetConfigOperation, &KScreen::GetConfigOperation::finished,
            this, [this](KScreen::ConfigOperation* op) {
        if (op->hasError()) {
            qDebug() << "Error getting initial configuration" << op->errorString();
            return;
        }
        if (mMonitoredConfig) {
            if(mMonitoredConfig->data()){
                KScreen::ConfigMonitor::instance()->removeConfig(mMonitoredConfig->data());
                for (const KScreen::OutputPtr &output : mMonitoredConfig->data()->outputs()) {
                    output->disconnect(this);
                }
                mMonitoredConfig->data()->disconnect(this);
            }
            mMonitoredConfig = nullptr;
        }

        mMonitoredConfig = std::unique_ptr<xrandrConfig>(new xrandrConfig(qobject_cast<KScreen::GetConfigOperation*>(op)->config()));
        mMonitoredConfig->setValidityFlags(KScreen::Config::ValidityFlag::RequireAtLeastOneEnabledScreen);
        monitorsInit();
    });
}

XrandrManager::~XrandrManager()
{

    if(mAcitveTime)
        delete mAcitveTime;
    if(mXrandrSetting)
        delete mXrandrSetting;
    if(mLoginInter)
        delete mLoginInter;
}

bool XrandrManager::XrandrManagerStart()
{
    USD_LOG(LOG_DEBUG,"Xrandr Manager Start");
    connect(mAcitveTime, &QTimer::timeout, this,&XrandrManager::StartXrandrIdleCb);
    mAcitveTime->start(2000);
    return true;
}

void XrandrManager::XrandrManagerStop()
{
     USD_LOG(LOG_DEBUG,"Xrandr Manager Stop");
}

/*查找触摸屏设备ID*/
static bool
find_touchscreen_device(Display* display, XIDeviceInfo *dev)
{
    int i, j;
    if (dev->use != XISlavePointer)
        return false;
    if (!dev->enabled)
    {
        qDebug("This device is disabled.");
        return false;
    }

    for (i = 0; i < dev->num_classes; i++)
    {
        if (dev->classes[i]->type == XITouchClass)
        {
            XITouchClassInfo *t = (XITouchClassInfo*)dev->classes[i];

            if (t->mode == XIDirectTouch)
            return true;
        }
    }
    return false;
}

/* Get device node for gudev
   node like "/dev/input/event6"
 */
unsigned char *
getDeviceNode (XIDeviceInfo devinfo)
{
    Atom  prop;
    Atom act_type;
    int  act_format;
    unsigned long nitems, bytes_after;
    unsigned char *data;

    prop = XInternAtom(QX11Info::display(), XI_PROP_DEVICE_NODE, False);
    if (!prop)
        return NULL;

    if (XIGetProperty(QX11Info::display(), devinfo.deviceid, prop, 0, 1000, False,
                      AnyPropertyType, &act_type, &act_format, &nitems, &bytes_after, &data) == Success)
    {
        return data;
    }

    XFree(data);
    return NULL;
}

/* Get touchscreen info */
GList *
getTouchscreen(Display* display)
{
    gint n_devices;
    XIDeviceInfo *devs_info;
    int i;
    GList *ts_devs = NULL;
    Display *dpy = QX11Info::display();
    devs_info = XIQueryDevice(dpy, XIAllDevices, &n_devices);

    for (i = 0; i < n_devices; i ++)
    {
        if (find_touchscreen_device(dpy, &devs_info[i]))
        {
           unsigned char *node;
           TsInfo *ts_info = g_new(TsInfo, 1);
           node = getDeviceNode (devs_info[i]);

           if (node)
           {
               ts_info->input_node = node;
               ts_info->dev_info = devs_info[i];
               ts_devs = g_list_append (ts_devs, ts_info);
           }
        }
    }
    return ts_devs;
}

bool checkMatch(int output_width,  int output_height,
                double input_width, double input_height)
{
    double w_diff, h_diff;

    w_diff = ABS (1 - ((double) output_width / input_width));
    h_diff = ABS (1 - ((double) output_height / input_height));

    printf("w_diff is %f, h_diff is %f\n", w_diff, h_diff);

    if (w_diff < MAX_SIZE_MATCH_DIFF && h_diff < MAX_SIZE_MATCH_DIFF)
        return true;
    else
        return false;
}

/* Here to run command xinput
   更新触摸屏触点位置
*/
void doAction (char *input_name, char *output_name)
{
    char buff[100];
    sprintf(buff, "xinput --map-to-output \"%s\" \"%s\"", input_name, output_name);

    printf("buff is %s\n", buff);

    QProcess::execute(buff);
}


void SetTouchscreenCursorRotation()
{
    int     event_base, error_base, major, minor;
    int     o;
    Window  root;
    int     xscreen;
    XRRScreenResources *res;
    Display *dpy = QX11Info::display();

    GList *ts_devs = NULL;

    ts_devs = getTouchscreen (dpy);

    if (!g_list_length (ts_devs))
    {
        fprintf(stdin, "No touchscreen find...\n");
        return;
    }

    GList *l = NULL;

    if (!XRRQueryExtension (dpy, &event_base, &error_base) ||
        !XRRQueryVersion (dpy, &major, &minor))
    {
        fprintf (stderr, "RandR extension missing\n");
        return;
    }

    xscreen = DefaultScreen (dpy);
    root = RootWindow (dpy, xscreen);

    if ( major >= 1 && minor >= 5)
    {
        res = XRRGetScreenResources (dpy, root);
        if (!res)
          return;

        for (o = 0; o < res->noutput; o++)
        {
            XRROutputInfo *output_info = XRRGetOutputInfo (dpy, res, res->outputs[o]);
            if (!output_info){
                fprintf (stderr, "could not get output 0x%lx information\n", res->outputs[o]);
                continue;
            }

            if (output_info->connection == 0) {
                int output_mm_width = output_info->mm_width;
                int output_mm_height = output_info->mm_height;

                for ( l = ts_devs; l; l = l->next)
                {
                    TsInfo *info = (TsInfo *)l -> data;
                    GUdevDevice *udev_device;
                    const char *udev_subsystems[] = {"input", NULL};

                    double width, height;
                    GUdevClient *udev_client = g_udev_client_new (udev_subsystems);
                    udev_device = g_udev_client_query_by_device_file (udev_client,
                                                                      (const gchar *)info->input_node);

                    if (udev_device && g_udev_device_has_property (udev_device,
                                                                   "ID_INPUT_WIDTH_MM"))
                    {
                        width = g_udev_device_get_property_as_double (udev_device,
                                                                    "ID_INPUT_WIDTH_MM");
                        height = g_udev_device_get_property_as_double (udev_device,
                                                                     "ID_INPUT_HEIGHT_MM");

                        if (checkMatch(output_mm_width, output_mm_height, width, height)){
                            doAction(info->dev_info.name, output_info->name);
                        }
                    }
                    g_clear_object (&udev_client);
                }
            }
        }
    }else {
        g_list_free(ts_devs);
        fprintf(stderr, "xrandr extension too low\n");
        return;
    }
    g_list_free(ts_devs);
}

void XrandrManager::orientationChangedProcess(Qt::ScreenOrientation orientation)
{
    SetTouchscreenCursorRotation();
}

/*监听旋转键值回调 并设置旋转角度*/
void XrandrManager::RotationChangedEvent(QString key)
{
    int angle;

    if(key != XRANDR_ROTATION_KEY)
        return;

    angle = mXrandrSetting->getEnum(XRANDR_ROTATION_KEY);
    qDebug()<<"angle = "<<angle;

    const KScreen::OutputList outputs = mMonitoredConfig->data()->outputs();
    for(auto output : outputs){
        if (!output->isConnected() || !output->isEnabled() || !output->currentMode()) {
            continue;
        }
        output->setRotation(static_cast<KScreen::Output::Rotation>(angle));
        qDebug()<<output->rotation() <<output->name();
    }
    doApplyConfig(mMonitoredConfig->data());
}

void XrandrManager::applyIdealConfig()
{
    const bool showOsd = mMonitoredConfig->data()->connectedOutputs().count() > 1 && !mStartingUp;
    //qDebug()<<"show Sod  is "<<showOsd;
}

void XrandrManager::doApplyConfig(const KScreen::ConfigPtr& config)
{
    //qDebug() << "Do set and apply specific config";
    auto configWrapper = std::unique_ptr<xrandrConfig>(new xrandrConfig(config));
    configWrapper->setValidityFlags(KScreen::Config::ValidityFlag::RequireAtLeastOneEnabledScreen);

    doApplyConfig(std::move(configWrapper));
}

void XrandrManager::doApplyConfig(std::unique_ptr<xrandrConfig> config)
{
    mMonitoredConfig = std::move(config);
    //monitorsInit();
    refreshConfig();
    primaryScreenChange();
}

void XrandrManager::refreshConfig()
{
}

void XrandrManager::saveCurrentConfig()
{

}

void XrandrManager::configChanged()
{

}

void XrandrManager::setMonitorForChanges(bool enabled)
{
    if (mMonitoring == enabled) {
        return;
    }

    //qDebug() << "Monitor for changes: " << enabled;
    mMonitoring = enabled;
    if (mMonitoring) {
        connect(KScreen::ConfigMonitor::instance(), &KScreen::ConfigMonitor::configurationChanged,
                this, &XrandrManager::configChanged, Qt::UniqueConnection);
    } else {
        disconnect(KScreen::ConfigMonitor::instance(), &KScreen::ConfigMonitor::configurationChanged,
                   this, &XrandrManager::configChanged);
    }
}

void XrandrManager::applyKnownConfig(bool state)
{

}

void XrandrManager::init_primary_screens (KScreen::ConfigPtr Config)
{

}

void XrandrManager::applyConfig()
{


}

void XrandrManager::outputConnectedChanged()
{

}

void XrandrManager::outputAdded(const KScreen::OutputPtr &output)
{
    USD_LOG(LOG_DEBUG,".");

}

void XrandrManager::outputRemoved(int outputId)
{
     USD_LOG(LOG_DEBUG,".");

}

void XrandrManager::primaryOutputChanged(const KScreen::OutputPtr &output)
{
    USD_LOG(LOG_DEBUG,".");
}

void XrandrManager::primaryScreenChange()
{
    USD_LOG(LOG_DEBUG,".");
}

void XrandrManager::callMethod(QRect geometry, QString name)
{
    Q_UNUSED(geometry);
    Q_UNUSED(name);
}


/*
 *
 *接入时没有配置文件的处理流程：
 * 单屏：最优分辨率。
 * 多屏幕：镜像模式。
 *
*/
void XrandrManager::outputConnectedWithoutConfigFile(KScreen::Output *newOutput, char outputCount)
{
    QString bigestModeId;
    QString bigestPrimaryModeId;
    int rtResolution = 0;
    int bigestResolution = 0;
    bool hadPrimary = false;
    if (1 == outputCount) {//单屏接入时需要设置模式，主屏
        newOutput->setCurrentModeId(newOutput->preferredModeId());
        newOutput->setPrimary(true);
    } else {

        Q_FOREACH(const KScreen::OutputPtr &output,mMonitoredConfig->data()->outputs()){
            if (output->isPrimary()){
                hadPrimary = true;
            }
        }

        if (!hadPrimary){
            Q_FOREACH(const KScreen::OutputPtr &output,mMonitoredConfig->data()->outputs()){
                 if (output->isConnected() && output->isEnabled()){
                     output->setPrimary(true);

                     auto *op = new KScreen::SetConfigOperation(mMonitoredConfig->data());
                     op->exec();

                     USD_LOG(LOG_DEBUG,"can't find primary screen set %s is primary..",output->name().toLatin1().data());
                     break;
                 }
            }
        }

        Q_FOREACH(const KScreen::OutputPtr &output,mMonitoredConfig->data()->outputs()){
            USD_LOG_SHOW_OUTPUT(output);

            if (output->isPrimary()){
                Q_FOREACH (auto primaryMode, output->modes()) {
                    Q_FOREACH (auto newOutputMode, newOutput->modes()) {

                        if (primaryMode->size().width() == newOutputMode->size().width() &&
                                primaryMode->size().height() == newOutputMode->size().height())
                        {
//                            rtModeId = newOutputMode;

                            rtResolution = primaryMode->size().width() * primaryMode->size().height();

                            if (rtResolution > bigestResolution){
                                bigestModeId = newOutputMode->id();
                                bigestPrimaryModeId = primaryMode->id();
                            }
                            USD_LOG(LOG_DEBUG,"good..p_width:%d p_height:%d n_width:%d n_height:%d",
                                    primaryMode->size().width(),primaryMode->size().height(),
                                    newOutputMode->size().width(), newOutputMode->size().height());
                        }/*else {
                            USD_LOG(LOG_DEBUG,"bad..p_width:%d p_height:%d n_width:%d n_height:%d",
                                    primaryMode->size().width(),primaryMode->size().height(),
                                    newOutputMode->size().width(), newOutputMode->size().height());
                        }*/

                    }//end  Q_FOREACH (auto newOutputMode, newOutput->modes())
                }//end  Q_FOREACH (auto primaryMode, output->modes())
            }//end if (output->isPrimary())
            else {
                USD_LOG(LOG_DEBUG,"%s is't primary..",output->name().toLatin1().data());
            }
        }//end  Q_FOREACH(const KScreen::OutputPtr &output,mMonitoredConfig->data()->outputs()




        if (!bigestResolution) {
            USD_LOG(LOG_DEBUG, "can't find the same resolution in outputs.so use bigest resolution.");
            auto *op = new KScreen::SetConfigOperation(mMonitoredConfig->data());
            op->exec();
            Q_FOREACH(const KScreen::OutputPtr &output, mMonitoredConfig->data()->outputs()){
                if (output->isPrimary()){
                    output->setCurrentModeId(output->preferredModeId());
                    USD_LOG(LOG_DEBUG, "primary output mode:%s",output->preferredModeId().toLatin1().data());
                    break;
                }
            }

             newOutput->setEnabled(newOutput->isConnected());
             newOutput->setCurrentModeId(newOutput->preferredModeId());
             USD_LOG(LOG_DEBUG, "newOutput output mode:%s",newOutput->preferredModeId().toLatin1().data());
             USD_LOG_SHOW_OUTPUT(newOutput);

        } else {

            Q_FOREACH(const KScreen::OutputPtr &output, mMonitoredConfig->data()->outputs()){
                if (output->isPrimary()){
                    output->setCurrentModeId(bigestPrimaryModeId);
                    break;
                }
            }
            newOutput->setEnabled(newOutput->isConnected());
            newOutput->setCurrentModeId(bigestModeId);
//            USD_LOG(LOG_DEBUG,"");
            USD_LOG_SHOW_OUTPUT(newOutput);
        }

    }
}

void XrandrManager::outputChangedHandle(KScreen::Output *senderOutput)
{
    char outputConnectCount = 0;
    USD_LOG_SHOW_OUTPUT(senderOutput);

    Q_FOREACH(const KScreen::OutputPtr &output,mMonitoredConfig->data()->outputs()) {
        USD_LOG_SHOW_OUTPUT(output);

        if (output->name()==senderOutput->name()) {//这里只设置connect，enbale由配置设置。
            output->setConnected(senderOutput->isConnected());

            if (0 == output->modes().count()) {
                output->setModes(senderOutput->modes());
            }
           USD_LOG(LOG_DEBUG,"find and set it....%s",output->name().toLatin1().data());
       }

        if (output->isConnected()) {
            outputConnectCount++;
        }
    }

    if (false == mMonitoredConfig->fileExists()) {
        USD_LOG(LOG_DEBUG,"config  %s is't exists.connect:%d.", mMonitoredConfig->filePath().toLatin1().data(),outputConnectCount);
        if (senderOutput->isConnected()) {
            senderOutput->setEnabled(senderOutput->isConnected());
        }

        outputConnectedWithoutConfigFile(senderOutput, outputConnectCount);

        mMonitoredConfig->writeFile(true);//首次接入，

    } else {

        mMonitoredConfig = mMonitoredConfig->readFile(false);

        USD_LOG(LOG_DEBUG,"%s it...FILE:%s",senderOutput->isConnected()? "Enable":"Disable",mMonitoredConfig->filePath().toLatin1().data());
    }

    Q_FOREACH(const KScreen::OutputPtr &output,mMonitoredConfig->data()->outputs()) {
        USD_LOG_SHOW_OUTPUT(output);
    }
    auto *op = new KScreen::SetConfigOperation(mMonitoredConfig->data());
    op->exec();

}

void XrandrManager::monitorsInit()
{
    char connectedOutputCount = 0;
    if (mConfig) {
        KScreen::ConfigMonitor::instance()->removeConfig(mConfig);
        for (const KScreen::OutputPtr &output : mConfig->outputs()) {
            output->disconnect(this);
        }
        mConfig->disconnect(this);
    }

    mConfig = std::move(mMonitoredConfig->data());

    USD_LOG(LOG_DEBUG,"config...:%s.",mMonitoredConfig->filePath().toLatin1().data());


    for (const KScreen::OutputPtr &output: mConfig->outputs()) {
        USD_LOG_SHOW_OUTPUT(output);

        if (output->isConnected()){
            connectedOutputCount++;
        }

        connect(output.data(), &KScreen::Output::isConnectedChanged, this, [this](){
        });

        connect(output.data(), &KScreen::Output::outputChanged, this, [this](){
            KScreen::Output *senderOutput = static_cast<KScreen::Output*> (sender());
            outputChangedHandle(senderOutput);
        });

        connect(output.data(), &KScreen::Output::scaleChanged, this, [this](){
            KScreen::Output *output1 = static_cast<KScreen::Output*> (sender());
            USD_LOG(LOG_DEBUG,"scaleChanged:%s",output1->name().toLatin1().data());
            USD_LOG_SHOW_OUTPUT(output1);
        });

        connect(output.data(), &KScreen::Output::clonesChanged, this, [this](){
            KScreen::Output *output1 = static_cast<KScreen::Output*> (sender());
            USD_LOG(LOG_DEBUG,"clonesChanged:%s",output1->name().toLatin1().data());
            USD_LOG_SHOW_OUTPUT(output1);
        });

        connect(output.data(), &KScreen::Output::posChanged, this, [this](){
            KScreen::Output *output1 = static_cast<KScreen::Output*> (sender());
            USD_LOG(LOG_DEBUG,"posChanged:%s",output1->name().toLatin1().data());
        });

        connect(output.data(), &KScreen::Output::sizeChanged, this, [this](){
            KScreen::Output *output1 = static_cast<KScreen::Output*> (sender());
            USD_LOG(LOG_DEBUG,"sizeChanged:%s",output1->name().toLatin1().data());
            USD_LOG_SHOW_OUTPUT(output1);
        });

    }

    KScreen::ConfigMonitor::instance()->addConfig(mConfig);

    connect(mMonitoredConfig->data().data(), &KScreen::Config::outputAdded,
            this, &XrandrManager::outputAdded), Qt::UniqueConnection;
    connect(mConfig.data(), &KScreen::Config::outputRemoved,
            this, &XrandrManager::outputRemoved,
            static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));
    connect(mConfig.data(), &KScreen::Config::primaryOutputChanged,
            this, &XrandrManager::primaryOutputChanged);

    if (mMonitoredConfig->fileExists()){
        mMonitoredConfig = mMonitoredConfig->readFile(false);
        auto *op = new KScreen::SetConfigOperation(mMonitoredConfig->data());
        op->exec();
    } else {

        for (const KScreen::OutputPtr &output: mMonitoredConfig->data()->outputs()) {
            USD_LOG_SHOW_OUTPUT(output);
            if (1==connectedOutputCount){
                outputChangedHandle(output.data());
                break;
            }
            else {
                if (output->isConnected() && false == output->isEnabled()) {
                    outputChangedHandle(output.data());
                }
            }
        }

    }

    for (const KScreen::OutputPtr &output: mMonitoredConfig->data()->outputs()) {
        USD_LOG_SHOW_OUTPUT(output);
    }
}

void XrandrManager::mPrepareForSleep(bool state)
{

}

/**
 * @brief XrandrManager::StartXrandrIdleCb
 * 开始时间回调函数
 */
void XrandrManager::StartXrandrIdleCb()
{
    mAcitveTime->stop();
//    SetTouchscreenCursorRotation();

    if(!mScreen)
        mScreen = QApplication::screens().at(0);

//    connect(mXrandrSetting,SIGNAL(changed(QString)),this,
//            SLOT(RotationChangedEvent(QString)));

    connect(mScreen, &QScreen::orientationChanged, this,
            &XrandrManager::orientationChangedProcess);

     USD_LOG(LOG_DEBUG,"StartXrandrIdleCb ok");
     QMetaObject::invokeMethod(this, "getInitialConfig", Qt::QueuedConnection);
}
