/*
 *   Bespin Mac-a-like XBar Plasmoid
 *   Copyright 2007-2012 by Thomas Lübking <thomas.luebking@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef QNETCTL_ADAPTOR_H
#define QNETCTL_ADAPTOR_H

#include <QDBusAbstractAdaptor>
#include "QNetCtl.h"

class QNetCtlAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.archlinux.qnetctl")

private:
    QNetCtl *myNetCtl;

public:
    QNetCtlAdaptor(QNetCtl *netCtl) : QDBusAbstractAdaptor(netCtl), myNetCtl(netCtl) {
        connect(netCtl, SIGNAL(request(QString, QString)), SIGNAL(request(QString, QString)));
    }

public slots:
    Q_NOREPLY void reply(QString tag, QString information) { myNetCtl->reply(tag, information); }
//     Q_NOREPLY void triggerRequest(QString tag, QString information) { emit request(tag, information); }
signals:
    void request(QString info, QString tag);
};

#endif // QNETCTL_ADAPTOR_H
