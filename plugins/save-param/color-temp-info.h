/* -*- Mode: C++; indent-tabs-mode: nil; tab-width: 4 -*-
 * -*- coding: utf-8 -*-
 *
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

#ifndef COLORTEMPINFO_H
#define COLORTEMPINFO_H

#include <QHash>
#include <QVariant>
#include <QString>
#include <QDBusArgument>

struct ColorInfo {
    QString arg;
    QDBusVariant out;
};

QDBusArgument &operator<<(QDBusArgument &argument, const ColorInfo &mystruct)
{
    argument.beginStructure();
    argument << mystruct.arg << mystruct.out;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ColorInfo &mystruct)
{
    argument.beginStructure();
    argument >> mystruct.arg >> mystruct.out;
    argument.endStructure();
    return argument;
}

Q_DECLARE_METATYPE(ColorInfo)

#endif // COLORTEMPINFO_H
