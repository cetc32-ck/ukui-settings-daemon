/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2019 Tianjin KYLIN Information Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "widget.h"
#include "ui_widget.h"

#include <QDebug>
#include <QScreen>
#include <QDBusInterface>
#include <QDBusConnection>

#include <kwindowsystem.h>
#include <QGraphicsEffect>
#include <QPainter>
#include <QStyleOption>
#include <KWindowEffects>
#include <QBitmap>


#include "expendbutton.h"
#include "qtsingleapplication.h"
#include "xeventmonitor.h"
#include "clib-syslog.h"

#define TITLEHEIGHT 90
#define OPTIONSHEIGHT 70
#define BOTTOMHEIGHT 38

#define FIRSTSCREENID 0
#define CLONESCREENID 1
#define EXTENEDSCREENID 2
#define OTHERSCREENID 3
#define ALLMODESID 4


#define XSETTINGS_SCHEMA            "org.ukui.SettingsDaemon.plugins.xsettings"
#define XSETTINGS_KEY_SCALING       "scaling-factor"

#define QT_THEME_SCHEMA             "org.ukui.style"



Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);
    m_superPresss = false;

    metaEnum = QMetaEnum::fromType<UsdBaseClass::eScreenMode>();
}

Widget::~Widget()
{
    delete ui;
    delete ukcciface;
    delete m_scaleSetting;
}

void Widget::beginSetup(){
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    setAttribute(Qt::WA_TranslucentBackground, true);

    ukcciface = new QDBusInterface("org.ukui.ukcc.session",
                                   "/",
                                   "org.ukui.ukcc.session.interface",
                                   QDBusConnection::sessionBus());
    m_scaleSetting = new QGSettings(XSETTINGS_SCHEMA);
    m_scale = m_scaleSetting->get(XSETTINGS_KEY_SCALING).toDouble();

    m_styleSettings = new QGSettings(QT_THEME_SCHEMA);

    /* 不在任务栏显示图标 */
    KWindowSystem::setState(winId(), NET::SkipTaskbar | NET::SkipPager);

    QScreen * pScreen = QGuiApplication::screens().at(0);
    QPoint point = pScreen->geometry().center();
    move(point.x() - width()/2, point.y() - height()/2);

    initData();

    setupComponent();
    setupConnect();

    initCurrentStatus(getCurrentStatus());

    connect(XEventMonitor::instance(), SIGNAL(buttonPress(int,int)),
            this, SLOT(XkbButtonEvent(int,int)));
    XEventMonitor::instance()->start();
}

void Widget::initData(){
    btnsGroup = new QButtonGroup;

    gtk_init(NULL, NULL);

    //Monitor init
    kScreen = mate_rr_screen_new (gdk_screen_get_default (), NULL);
//    kConfig = mate_rr_config_new_current (kScreen, NULL);

}

void Widget::setupComponent(){

    int h = TITLEHEIGHT + OPTIONSHEIGHT * ALLMODESID + BOTTOMHEIGHT;
    QStringList btnTextList;
    QStringList btnImg;

    btnTextList<<"First Screen";
    btnTextList<<"Clone Screen";
    btnTextList<<"Extend Screen";
    btnTextList<<"Vice Screen";

    btnImg<<":/img/main.png";
    btnImg<<":/img/clone.png";
    btnImg<<":/img/extend.png";
    btnImg<<":/img/vice.png";

    setFixedWidth(384);
    setFixedHeight(h);

    const QString style = m_styleSettings->get("style-name").toString();

    ui->outputPrimaryTip->hide();

    for (int i = 0; i < ALLMODESID; i++){
        ExpendButton * btn = new ExpendButton();
        btn->setFixedHeight(70);
        btnsGroup->addButton(btn, i);

        btn->setSign(i % 2,style);
        btn->setBtnText(tr(btnTextList[i].toLatin1().data()));
        btn->setBtnLogo(btnImg[i],style);

        ui->btnsVerLayout->addWidget(btn);
    }

    m_qssDark = ("QFrame#titleFrame{background: #40131314; border: none; border-top-left-radius: 24px; border-top-right-radius: 24px;}"\
                   "QFrame#bottomFrame{background: #40131314; border: none; border-bottom-left-radius: 24px; border-bottom-right-radius: 24px;}"\
                   "QFrame#splitFrame{background: #99000000; border: none;}"\
                   "QLabel#titleLabel{color: #FFFFFF; font-size:24px;}"\
                   "QLabel#outputPrimaryTip{color: #60FFFFFF; }"\
                   "QLabel#outputName{color: #FFFFFF; }"\
                   "QLabel#outputDisplayName{color: #60FFFFFF; }"\
                );

    m_qssDefault = ("QFrame#titleFrame{background: #40F5F5F5; border: none; border-top-left-radius: 24px; border-top-right-radius: 24px;}"\
                   "QFrame#bottomFrame{background: #40F5F5F5; border: none; border-bottom-left-radius: 24px; border-bottom-right-radius: 24px;}"\
                   "QFrame#splitFrame{background: #99000000; border: none;}"\
                   "QLabel#titleLabel{color: #232426; font-size:24px;}"\
                   "QLabel#outputPrimaryTip{color: #60FFFFFF; }"\
                   "QLabel#outputName{color: #232426; }"\
                   "QLabel#outputDisplayName{color: #60FFFFFF; }"\
                );
}

void Widget::setupConnect(){

    connect(btnsGroup, static_cast<void(QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, [=](int id){
        /* 获取旧选项 */
        for (QAbstractButton * button : btnsGroup->buttons()){
            ExpendButton * btn = dynamic_cast<ExpendButton *>(button);
//            qDebug() << "old index: " << btn->getBtnChecked();
            int index = btnsGroup->id(button);
            if (index == id && btn->getBtnChecked()){
                   close();
            }
        }

        setScreenModeByDbus(metaEnum.key(id));
        close();
    });

}


int Widget::getCurrentStatus()
{
    QDBusMessage message = QDBusMessage::createMethodCall("org.ukui.SettingsDaemon",
                                                          "/org/ukui/SettingsDaemon/wayland",
                                                          "org.ukui.SettingsDaemon.wayland",
                                                          "getScreenMode");
    QList<QVariant> args;
    args.append(qAppName());
    message.setArguments(args);

    QDBusMessage response = QDBusConnection::sessionBus().call(message);

    if (response.type() == QDBusMessage::ReplyMessage)
    {
        if(response.arguments().isEmpty() == false) {
            int value = response.arguments().takeFirst().toInt();
            USD_LOG(LOG_DEBUG, "get mode :%s", metaEnum.key(value));

            return value;
        }
    } else {
        USD_LOG(LOG_DEBUG, "called failed cuz:%s", message.errorName().toLatin1().data());
    }

    return 0;
}

void Widget::initCurrentStatus(int id){
    //set all no checked
    for (QAbstractButton * button : btnsGroup->buttons()){
        ExpendButton * btn = dynamic_cast<ExpendButton *>(button);

        btn->setBtnChecked(false);

        if (id == btnsGroup->id(button)){
            btn->setBtnChecked(true);
            btn->setChecked(true);
        }
    }

    if (id == -1){
        ExpendButton * btn1 = dynamic_cast<ExpendButton *>(btnsGroup->button(0));
        btn1->setChecked(true);
    }
}


void Widget::nextSelectedOption(){
    int current = btnsGroup->checkedId();
    int next;


    next = current == ALLMODESID - 1 ? 0 : current + 1;

    ExpendButton * btn = dynamic_cast<ExpendButton *>(btnsGroup->button(next));
    btn->setChecked(true);
}

void Widget::lastSelectedOption(){
    int current = btnsGroup->checkedId();
    int last;

    /* no button checked */
    if (current == -1)
        current = ALLMODESID;

    last = current == 0 ? ALLMODESID - 1 : current - 1;

    ExpendButton * btn = dynamic_cast<ExpendButton *>(btnsGroup->button(last));
    btn->setChecked(true);
}

void Widget::confirmCurrentOption(){
    int current = btnsGroup->checkedId();

    if (current == -1)
        return;

    ExpendButton * btn = dynamic_cast<ExpendButton *>(btnsGroup->button(current));
//    btn->click();
    btn->animateClick();
}

void Widget::closeApp(){
    close();
}

void Widget::setScreenModeByDbus(QString modeName)
{
    QDBusMessage message = QDBusMessage::createMethodCall("org.ukui.SettingsDaemon",
                                                          "/org/ukui/SettingsDaemon/wayland",
                                                          "org.ukui.SettingsDaemon.wayland",
                                                          "setScreenMode");
    QList<QVariant> args;

    args.append(modeName);
    args.append(qAppName());
    message.setArguments(args);

    QDBusConnection::sessionBus().send(message);
}





void Widget::msgReceiveAnotherOne(const QString &msg){
//    qDebug() << "another one " << msg;
    nextSelectedOption();
}

void Widget::receiveButtonClick(int x, int y){
    qDebug() << "receive button press " << x << y;
    if (!this->geometry().contains(x, y)){
        close();
    }

}

void Widget::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
        case Qt::Key_Up:
            lastSelectedOption();
            break;
        case Qt::Key_Down:

            nextSelectedOption();
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            confirmCurrentOption();
            break;
        case Qt::Key_Meta:
        case Qt::Key_Super_L:
            m_superPresss = true;
            close();
            break;
        case Qt::Key_Display:
            nextSelectedOption();

            break;

        case Qt::Key_P:
            if(m_superPresss)
            {
                nextSelectedOption();

            }else
            {
                close();
            }
            break;
        default:
            close();
            break;
    }
}

void Widget::keyReleaseEvent(QKeyEvent *event)
{
        switch (event->key())
        {
            case Qt::Key_Meta:
            m_superPresss = false;

                break;
            default:
                break;
        }
}

void Widget::XkbButtonEvent(int x,int y)
{
    receiveButtonClick( x/m_scale, y/m_scale);
}

void Widget::showEvent(QShowEvent* event)
{
    if(m_styleSettings->get("style-name").toString() == "ukui-default")
    {
        setStyleSheet(m_qssDefault);
    }
    else
    {
        setStyleSheet(m_qssDark);
    }
}



void Widget::paintEvent(QPaintEvent *event)
{
    QRect rect = this->rect();
    QPainterPath path;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);  //反锯齿;
    painter.setPen(Qt::transparent);
    qreal radius=24;
    path.moveTo(rect.topRight() - QPointF(radius, 0));
    path.lineTo(rect.topLeft() + QPointF(radius, 0));
    path.quadTo(rect.topLeft(), rect.topLeft() + QPointF(0, radius));
    path.lineTo(rect.bottomLeft() + QPointF(0, -radius));
    path.quadTo(rect.bottomLeft(), rect.bottomLeft() + QPointF(radius, 0));
    path.lineTo(rect.bottomRight() - QPointF(radius, 0));
    path.quadTo(rect.bottomRight(), rect.bottomRight() + QPointF(0, -radius));
    path.lineTo(rect.topRight() + QPointF(0, radius));
    path.quadTo(rect.topRight(), rect.topRight() + QPointF(-radius, -0));

    painter.setBrush(this->palette().base());
    painter.setPen(Qt::transparent);
    painter.setOpacity(0.75);
    painter.drawPath(path);

    KWindowEffects::enableBlurBehind(this->winId(), true, QRegion(path.toFillPolygon().toPolygon()));
    QWidget::paintEvent(event);
}





