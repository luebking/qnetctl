/**************************************************************************
*   Copyright (C) 2013 by Thomas Luebking                                 *
*   thomas.luebking@gmail.com                                             *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/

#ifndef Q_NET_CTL_H
#define Q_NET_CTL_H

class QPushButton;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
#include <QList>
#include <QMap>
#include <QTabWidget>

class Connection
{
public:
    enum Type { Unknown = 0, Ethernet, Wireless, WEP, WPA, WPA1, WPA2 };
    Connection() : type(Unknown), quality(0), active(false), adHoc(false) {}
    Connection(const Connection &other);
    explicit Connection(QString profile);
    Type type;
    QString SSID, MAC, description, interface, profile, ipResolution, key;
    int quality;
    bool active, adHoc;
};

namespace Ui {
    class Settings;
    class IPConfig;
}

class QNetCtl : public QTabWidget
{
    Q_OBJECT
public:
    QNetCtl();
//     ~QNetCtl();
protected:
    void closeEvent(QCloseEvent *event);
private:
    void checkConnections();
    QTreeWidgetItem *currentItem() const;
    void query(QString cmd, const char *slot);
    void readConfig();
    void updateTree();
private slots:
    void buildTree();
    void checkDevices();
    void connectNetwork();
    bool editProfile();
    void expandCurrent();
    void forgetProfile();
    void readProfiles();
    void scanWifi();
    void parseDevices();
    void parseProfiles();
    void parseWifiDevs();
    void parseWifiScan();
    void showSelected(QTreeWidgetItem *, QTreeWidgetItem*);
    bool writeProfile(QTreeWidgetItem *item, QString key);
    void verifyPath();
private:
    QTreeWidget *myNetworks;
    QPushButton *myConnectButton, *myForgetButton, *myEditButton;
    QList<Connection> myProfiles, myWLANs;
    QMap<QString, bool> myDevices;
    QTimer *myUpdateTimer, *myRescanTimer;
    int iWaitForIwScan;
    Ui::Settings *mySettings;
    Ui::IPConfig *myProfileConfig;
};

#endif // Q_NET_CTL_H