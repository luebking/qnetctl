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

#include "QNetCtl.h"
#include "QNetCtl_dbus.h"
#include "ui_ipconfig.h"
#include "ui_settings.h"

#include <QAbstractItemDelegate>
#include <QApplication>
#include <QDBusConnection>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QIcon>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <signal.h>

#include <QtDebug>


#define READ_STDOUT(_var_, _error_)\
    QProcess *proc = static_cast<QProcess*>(sender());\
    if (proc->exitStatus() != QProcess::NormalExit || proc->exitCode()) {\
        qDebug() << _error_ << proc->exitStatus() << proc->exitCode();\
        return;\
    }\
    QString _var_(QString::fromLocal8Bit(proc->readAllStandardOutput()))

// #define TOOL(_T_) mySettings->_T_->text()

#include "paths.h"

enum Roles { IsDetailRole = Qt::UserRole + 1, TypeRole, QualityRole, ConnectedRole, AdHocRole,
             MacRole, ProfileRole, SsidRole, DescriptionRole, InterfaceRole, IPRole, KeyRole,
             AutoconnectRole };

Connection::Connection(const Connection &other)
{
    type         = other.type;
    SSID         = other.SSID;
    profile      = other.profile;
    quality      = other.quality;
    MAC          = other.MAC;
    active       = other.active;
    description  = other.description;
    interface    = other.interface;
    adHoc        = other.adHoc;
    ipResolution = other.ipResolution;
    key          = other.key;
    autoConnect  = other.autoConnect;
}

Connection::Connection(QString p)
{
    profile = p;
    autoConnect = true;
    type = Unknown;
    active = false;
    quality = 0;
    adHoc = false;
    QFile file(gs_profilePath + profile);
    if (!file.exists()) {
        qDebug() << "attempted to read non existing profile:" << profile;
        return;
    }
    if (!file.open(QIODevice::ReadOnly|QIODevice::Text)) {
        qDebug() << "attempted to read protected profile:" << profile;
        return;
    }
    Type sec = Unknown;
    while (!file.atEnd()) {
        QString line = file.readLine();
        line = line.section('#', 0, 0).trimmed(); // drop commented stuff
        if (line.startsWith("Description")) {
            description = line.section('=', 1);
        } else if (line.startsWith("Connection")) {
            QString con(line.section('=', 1));
            if (con == "ethernet") {
                quality = 100;
                type = Ethernet;
            } else if (con == "wireless") {
                type = Wireless;
            }
            // else if ... TODO: more useless connection types
        } else if (line.startsWith("Interface")) {
            interface = line.section('=', 1);
        } else if (line.startsWith("ESSID")) {
            SSID = line.section('=', 1);
        } else if (line.startsWith("Security")) {
            QString secs(line.section('=', 1));
            if (secs == "wep")
                sec = WEP;
            else if (secs == "wpa")
                sec = WPA;
        } else if (line.startsWith("Key")) {
            key = line.section('=', 1);
        } else if (line.startsWith("IP")) {
            QString ip = line.section('=', 1).trimmed();
            if (ipResolution.isEmpty() || ip == "dhcp") // dhcp trumps Address & Gateway definition
                ipResolution = ip;
        } else if (line.startsWith("Address")) {
            if (ipResolution != "dhcp")
                ipResolution.prepend(line.section('=', 1).trimmed());
        } else if (line.startsWith("Gateway")) {
            if (ipResolution != "dhcp")
                ipResolution.append(';' + line.section('=', 1).trimmed());
        } else if (line.startsWith("ExcludeAuto")) {
            autoConnect = line.section('=', 1).trimmed() != "yes";
        } else if (line.startsWith("Priority")) {
            // int = line.section('=', 1).trimmed().toInt();
            void(0);
        }
    }
    file.close();
    if (type == Wireless && sec)
        type = sec;
}

static inline QColor mix(QColor c1, QColor c2)
{
    c1.setRed  ((c1.red()   + c2.red())  /2);
    c1.setGreen((c1.green() + c2.green())/2);
    c1.setBlue ((c1.blue()  + c2.blue()) /2);
    return c1;
}

class NetworkDelegate : public QAbstractItemDelegate
{
public:
    NetworkDelegate( QWidget *parent = 0 ) : QAbstractItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const
    {
        if (!option.rect.isValid())
            return;

        const QPalette &pal = option.palette;
        QRect rect = option.rect;
        const bool isDetails = idx.data(IsDetailRole).toBool();
        QModelIndex index = isDetails ? idx.parent() : idx;
        if ( option.state & QStyle::State_Selected ) {
            painter->fillRect( rect, pal.color(QPalette::Highlight) );
            painter->setPen( pal.color(QPalette::HighlightedText) );
        } else if (isDetails) {
            painter->setPen( pal.color(QPalette::Text) );
        } else {
            painter->fillRect( rect, pal.color(QPalette::Text) );
            painter->setPen( pal.color(QPalette::Base) );
        }

        QFont fnt = static_cast<QWidget*>(parent())->font();
        fnt.setBold(true);

        if (isDetails)
            rect.adjust(16, 1, -4, -4);
        else
            rect.adjust(4, 0, -4, 0);
        int textFlags = Qt::TextSingleLine | Qt::TextHideMnemonic;
        if (isDetails) {
            QString left = index.data(MacRole).toString();
            if (left.isEmpty())
                left = tr("Device: ") + index.data(InterfaceRole).toString();
            else
                left = index.data(InterfaceRole).toString() + " -> " + left;
            painter->drawText(rect, textFlags | Qt::AlignLeft|Qt::AlignTop, left);
            painter->drawText(rect, textFlags | Qt::AlignLeft|Qt::AlignBottom, "IP: " + index.data(IPRole).toString());
            painter->drawText(rect, textFlags | Qt::AlignRight|Qt::AlignTop, index.data(SsidRole).toString());

            Connection::Type t = (Connection::Type)index.data(TypeRole).toInt();
            QString ps;
            QColor c = Qt::green;
            switch (t) {
                default:
                case Connection::Ethernet:
                    if (index.data(QualityRole).toInt() > 0) {
                        ps = "Wired";
                    } else {
                        ps = index.data(ProfileRole).toString().isEmpty() ? "Unconfigured wired connection" : "Unwired";
                        c = Qt::red;
                    }
                    break;
                case Connection::Wireless:
                    c = Qt::red;
                    ps = "Insecure";
                    break;
                case Connection::WEP:
                    c = Qt::yellow;
                    ps = "Security: WEP";
                    break;
                case Connection::WPA:
                    ps = "Security: WPA";
                    break;
                case Connection::WPA1:
                    ps = "Security: WPA1";
                    break;
                case Connection::WPA2:
                    ps = "Security: WPA2";
                    break;
            }
            painter->setFont(fnt);
            painter->setPen( mix(c, painter->pen().color()) );
            painter->drawText(rect, textFlags | Qt::AlignRight|Qt::AlignBottom, ps);
        } else {
            int quality = qMax(0, index.data(QualityRole).toInt());
            QString qualityString = QString::number(quality) + "%  ";
            int i = 0;
            for (; i < qRound(quality/20.0); ++i)
                qualityString += QChar(0x2605);
            for (; i < 5; ++i)
                qualityString += QChar(0x2606);
            painter->drawText(rect, textFlags | Qt::AlignRight, qualityString);

            fnt.setPointSize(fnt.pointSize() * 1.2);
            painter->setFont(fnt);
            textFlags |= Qt::AlignVCenter;
            QString name = QChar(index.data(AdHocRole).toBool() ? 0x21C4 : 0x2192) + QString(" ");
            name += " " + index.data().toString();
            if (index.data(ConnectedRole).toBool())
                name = name + " " + QChar(0x26A1);
            else if (!index.data(ProfileRole).toString().isEmpty())
                name = name + " " + QChar(0x2714);
            painter->drawText(rect, textFlags | Qt::AlignLeft, name);
        }
        painter->setFont(static_cast<QWidget*>(parent())->font());
    }

    QSize sizeHint( const QStyleOptionViewItem &option, const QModelIndex &index ) const
    {
        if (index.data(IsDetailRole).toBool())
            return QSize(128, QFontMetrics(option.font).height() * 2 + 5);
        return QSize(128, QFontMetrics(option.font).height() * 3 / 2);
    }
};


QNetCtl::QNetCtl() : QTabWidget(), iWaitForIwScan(0), myProfileConfig(0)
{
    new QNetCtlAdaptor(this);
    const QString service = "org.archlinux.qnetctl-" + QString::number(QCoreApplication::applicationPid());
    QDBusConnection::sessionBus().registerService(service);
    QDBusConnection::sessionBus().registerObject("/QNetCtl", this);

    setWindowTitle("QNetCtl");
    myUpdateTimer = new QTimer(this);
    myUpdateTimer->setInterval(250);
    myUpdateTimer->setSingleShot(true);
    connect (myUpdateTimer, SIGNAL(timeout()), SLOT(buildTree()));

    myRescanTimer = new QTimer(this);
    myRescanTimer->setInterval(8000); // rescan every 8 seconds
    myRescanTimer->setSingleShot(false);
    connect (myRescanTimer, SIGNAL(timeout()), SLOT(scanWifi()));
    connect (myRescanTimer, SIGNAL(timeout()), SLOT(checkDevices()));
    myRescanTimer->start();

    myAutoConnectUpdateTimer = new QTimer(this);
    myAutoConnectUpdateTimer->setInterval(30000); // wait 30 seconds, it's just for reboots etc.
    myAutoConnectUpdateTimer->setSingleShot(true);
    connect (myAutoConnectUpdateTimer, SIGNAL(timeout()), SLOT(updateAutoConnects()));

    QWidget *w;
    QIcon icn = QIcon::fromTheme("preferences-system-network");
    addTab(w = new QWidget(this), icn, icn.isNull() ? tr("Networks") : QString());
    setTabToolTip(0, tr("Networks"));
    QVBoxLayout *l = new QVBoxLayout(w);
    l->addWidget(myNetworks = new QTreeWidget(w));
    myNetworks->setCursor(Qt::PointingHandCursor);
    myNetworks->setExpandsOnDoubleClick(true);
    connect (myNetworks, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
                         SLOT(showSelected(QTreeWidgetItem*, QTreeWidgetItem*)));
    myNetworks->setRootIsDecorated(false);
    myNetworks->setIconSize( QSize(32, 32) );
    myNetworks->setHeaderHidden(true);
    myNetworks->setIndentation(0);
    myNetworks->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
    myNetworks->setAnimated( true );
    myNetworks->setItemDelegate(new NetworkDelegate(myNetworks));
    QHBoxLayout *hl = new QHBoxLayout;
    hl->addWidget(myForgetButton = new QPushButton(tr("Forget"), w));
    connect (myForgetButton, SIGNAL(clicked()), SLOT(forgetProfile()));
    hl->addWidget(myEditButton = new QPushButton(tr("Edit"), w));
    connect (myEditButton, SIGNAL(clicked()), SLOT(editProfile()));
    hl->addWidget(myConnectButton = new QPushButton(tr("Connect"), w));
    connect (myConnectButton, SIGNAL(clicked()), SLOT(connectNetwork()));
    l->addLayout(hl);

//     addTab(w = new QWidget(this), QIcon::fromTheme("network-wireless"), QString());
//     setTabToolTip(1, "Radio Devices");
    icn = QIcon::fromTheme("configure");
    addTab(w = new QWidget(this), icn, icn.isNull() ? tr("Configuration") : QString());

    setTabToolTip(2, tr("Configuration"));
    mySettings = new Ui::Settings;
    mySettings->setupUi(w);
    connect (mySettings->leverage, SIGNAL(textChanged(const QString &)), SLOT(verifyPath()));

    readConfig();
    QProcess *tool = new QProcess(this);
    QString leverage = mySettings->leverage->text();
    leverage.replace("%w", QString::number(winId())).replace("%p", QString::number(QCoreApplication::applicationPid()));
    tool->start(leverage + " " + TOOL(qnetctl) + " " + QString(getenv("DBUS_SESSION_BUS_ADDRESS")) +
                           " " + QDBusConnection::sessionBus().name() + " " + service, QIODevice::NotOpen);
    query(TOOL(systemctl) + " list-unit-files", SLOT(parseEnabledNetworks()));
    readProfiles();
    scanWifi();
}

#define WRITE_CMD(_S_, _C_)\
if (mySettings->_C_->text().isEmpty())\
    s.remove(_S_);\
else\
    s.setValue(_S_, mySettings->_C_->text())

void QNetCtl::closeEvent(QCloseEvent *event)
{
    QSettings s("QNetCtl");
    s.setValue("Width", width());
    s.setValue("Height", height());
    WRITE_CMD("Leverage", leverage);
    if (myAutoConnectUpdateTimer->isActive()) {
        myAutoConnectUpdateTimer->stop(); // shortcut
        if (!updateAutoConnects())
            return; // do not close, user shall fix his setup.
    }
    emit request("quit", "");
    QWidget::closeEvent(event);
}

#define READ_CMD(_S_, _D_, _C_)\
cmd = s.value(_S_, _D_).toString();\
if (!(cmd.isEmpty() || QFile::exists(cmd.section(" ", 0, 0)))) { \
    qDebug() << "Warning: "_S_" does not exist, must be absolute path!" << cmd;\
    cmd.clear();\
}\
mySettings->_C_->setText(cmd)

void QNetCtl::readConfig()
{
    QSettings s("QNetCtl");
    const int w = s.value("Width", 240).toInt();
    const int h = s.value("Height", 5*w/4).toInt();
    resize(w, h);
    QString cmd;
    READ_CMD("Leverage", QString(), leverage);
}

void QNetCtl::query(QString cmd, const char *slot)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove("LC_ALL");
    env.remove("LANG");
    QProcess *proc = new QProcess(this);
    proc->setProcessEnvironment(env);
    connect (proc, SIGNAL(finished(int, QProcess::ExitStatus)), slot);
    connect (proc, SIGNAL(finished(int, QProcess::ExitStatus)), proc, SLOT(deleteLater()));
    proc->start(cmd, QIODevice::ReadOnly);
}

void QNetCtl::connectNetwork()
{
    QTreeWidgetItem *item = currentItem();
    if (!item)
        return;
    QString profile = item->data(0, ProfileRole).toString();
    if (profile.isEmpty() && !editProfile()) {
        return;
    }
    setEnabled(false);
    emit request("switch_to_profile", profile);

}

void QNetCtl::checkDevices()
{
    if (!(currentIndex() || TOOL(ip).isEmpty()))
        query(TOOL(ip) + " link show", SLOT(parseDevices()));
}

QTreeWidgetItem *QNetCtl::currentItem() const
{
    QTreeWidgetItem *item = myNetworks->currentItem();
    if (!item)
        return NULL;
    if (item->parent() && item->parent() != myNetworks->invisibleRootItem())
        return item->parent();
    return item;
}

void QNetCtl::parseDevices()
{
    READ_STDOUT(links, "Failed to read network devices:");

    QStringList linkList = links.split('\n');
    links.clear();
    bool checkWifi = false;
    foreach (const QString &link, linkList) {
        if (link.contains("BROADCAST")) {
            const bool broken = link.contains("NO-CARRIER"); // dead ethernet
            QString interface = link.section(':', 1, 1).trimmed();
            if (!myDevices.contains(interface)) {
                myDevices.insert(interface, false);
                checkWifi = true; // could be Wireless, iw will tell
            }
            for (QList<Connection>::iterator it = myProfiles.begin(),
                                            end = myProfiles.end(); it != end; ++it) {
                if (it->interface == interface) {
                    if ((it->quality < 0) != broken)
                        it->quality = -(it->quality);
                    break;
                }
            }
        }
    }
    if (checkWifi && !TOOL(iw).isEmpty()) {
        query(TOOL(iw) + " dev", SLOT(parseWifiDevs()));
    }
    updateTree();
}

void QNetCtl::parseEnabledNetworks()
{
    static QRegExp  ifplugd_interface("netctl-ifplugd@.*\\.service"),
                    auto_interface("netctl-auto@.*\\.service");
    READ_STDOUT(services, "Failed to list systemd services");
    QStringList serviceList = services.split('\n');
    services.clear();
    myEnabledProfiles.clear();
    foreach (const QString &l, serviceList) {
        if (!l.trimmed().endsWith("enabled"))
            continue;
        QString line = l.section(" ", 0, 0);
        if (line.startsWith("netctl") && line.endsWith(".service")) { // there're also the slices
            if (!line.indexOf(ifplugd_interface) || !line.indexOf(auto_interface))
                myEnabledProfiles << line; // ".service" is illegal for netcfg
            else {
                line = line.section('@', 1);
                myEnabledProfiles << line.section(".service", 0, -2);
            }
        }
    }
    updateTree();
}

void QNetCtl::parseWifiDevs() {
    READ_STDOUT(wireless, "Failed to list wireless devices");
    QStringList wirelessList = wireless.split('\n');
    wireless.clear();
    foreach (const QString &l, wirelessList) {
        const QString line = l.trimmed();
        if (line.startsWith("Interface")) {
            const QString interface = line.section(' ', 1);
            if (!myDevices.value(interface, false)) {
                myDevices[interface] = true;
                ++iWaitForIwScan;
                // query is ok, this is triggered from parseDevices only if iw exists
                emit request("scan_wifi", interface);
            }
        }
    }
}


void QNetCtl::scanWifi()
{
    if (currentIndex() || iWaitForIwScan || TOOL(iw).isEmpty())
        return; // this can last depending on the wifi chip - don't trigger a new scan
    // TODO: ensure ip link set <dev> up
    for (QMap<QString, bool>::const_iterator it = myDevices.constBegin(),
                                            end = myDevices.constEnd(); it != end; ++it) {
        if (*it) {
            ++iWaitForIwScan;
            emit request("scan_wifi", it.key());
        }
    }
}

void QNetCtl::parseWifiScan(QString networks)
{
    --iWaitForIwScan;
    myNetworks->setEnabled(true);

    QStringList networkList = networks.split("\nBSS");

    if (networkList.isEmpty())
        return;

    myWLANs.clear();
    foreach (const QString &network, networkList) {
        QStringList networkFields = network.split('\n', QString::SkipEmptyParts);
        if (networkFields.isEmpty())
            continue;
        myWLANs << Connection();
        Connection &connection = myWLANs.last();

        connection.type = Connection::Wireless;
        bool first = true;
        foreach (const QString &f, networkFields) {
            const QString field = f.simplified();
            if (first) {
                connection.MAC = field;
                connection.MAC = connection.MAC.remove("BSS ").section(' ', 0, 0).section('(', 0, 0);
                first = false;
            } else if (field.startsWith("capability")) {
                if (field.contains(" Privacy "))
                    connection.type = qMax(connection.type, Connection::WEP);
                if (field.contains(" IBSS "))
                    connection.adHoc = true;
            } else if (field.startsWith("signal:")) {
                const double d = field.section(':', 1).trimmed().section(' ', 0, 0).toDouble();
                connection.quality = qMax(0, qMin(100, int(5*(d+90)))); // [-90,-70] -> [0,100]
            } else if (field.startsWith("SSID:")) {
                connection.SSID = field.section(':', 1).trimmed();
            } else if (field.startsWith("RSN:")) {
                connection.type = qMax(connection.type, Connection::WPA2);
            } else if (field.startsWith("WPA:")) {
                connection.type = qMax(connection.type, Connection::WPA1);
            }
        }
    }
    updateTree();
}

bool QNetCtl::editProfile()
{
    QTreeWidgetItem *item = currentItem();
    if (!item)
        return false;

    QDialog dlg;
    if (!myProfileConfig) {
        myProfileConfig = new Ui::IPConfig;
    }
    myProfileConfig->setupUi(&dlg);
    dlg.setWindowTitle(tr("Edit profile"));
    myProfileConfig->key->setFont(QFont("monospace"));
//     myProfileConfig->key->setEchoMode(QLineEdit::Password);
    myProfileConfig->staticGroup->hide();

    QString oldProfileName = item->data(0, ProfileRole).toString();
    myProfileConfig->profile->setText(oldProfileName);

    const QString ip = item->data(0, IPRole).toString();
    myProfileConfig->dhcp->setChecked(ip == "dhcp");
    if (!myProfileConfig->dhcp->isChecked()) {
        myProfileConfig->ipv4->setText(ip.section(';', 0));
        if (ip.contains(';'))
            myProfileConfig->gateway4->setText(ip.section(';', 1));
    }

    const int type = item->data(0, TypeRole).toInt();
    bool autoConnect = item->data(0, AutoconnectRole).toBool();
    myProfileConfig->autoConnect->setChecked(autoConnect);
    if (type < Connection::WEP) {
        myProfileConfig->key->hide();
        myProfileConfig->keyLabel->hide();
    } else if (type == Connection::WEP) {
        const QString key = item->data(0, KeyRole).toString();
        if (key.startsWith("\\\""))
            myProfileConfig->key->setText(key.mid(2));
        else
            myProfileConfig->key->setText(key);
        myProfileConfig->key->setToolTip(tr("The key is a 10 or 26 digit long hexadecimal (0-9,a-f) number.<br>"
                                            "It must match the <b>WEP</b> key stored in the accesspoint<br>"
                                            "<br><b>Example:</b> 1A23B4C56D<br>"));
    } else { // WPA
        myProfileConfig->key->setText(item->data(0, KeyRole).toString());
        myProfileConfig->key->setToolTip(tr("The key is a random string of alphanumeric and special chars.<br>"
                                            "It must match the <b>WPA</b> key stored in the accesspoint<br>"
                                            "<br><b>Example:</b> Th15K3y15N0t53cur3<br>"));
    }
    dlg.adjustSize();

    if (dlg.exec() && (type < Connection::WEP || !myProfileConfig->key->text().isEmpty())) {
        QString key = myProfileConfig->key->text();
        if (type == Connection::WEP && (key.length() == 10 || key.length() == 26)) // WEP hex key needs to be escaped
            key.prepend('"');
        if (key.startsWith('"')) // needs to be escaped
            key.prepend('\\');
        QString profile = myProfileConfig->profile->text();
        item->setData(0, ProfileRole, profile);
        if (autoConnect != myProfileConfig->autoConnect->isChecked()) {
            autoConnect = myProfileConfig->autoConnect->isChecked();
            item->setData(0, AutoconnectRole, autoConnect);
            myEnabledProfiles.removeAll(profile);
            myEnabledProfiles.removeAll(oldProfileName);
            if (autoConnect) {
                myEnabledProfiles << profile;
                emit request("enable_profile", oldProfileName);
                emit request("enable_profile", profile);
            } else {
                emit request("disable_profile", oldProfileName);
                emit request("disable_profile", profile);
            }
            autoConnect = true; // for update
        }
        if (myProfileConfig->dhcp->isChecked())
            item->setData(0, IPRole, "dhcp");
        else
            item->setData(0, IPRole, myProfileConfig->ipv4->text() + ';' + myProfileConfig->gateway4->text());
        writeProfile(item, key);
        if (autoConnect)
            myAutoConnectUpdateTimer->start();
        return true;
    }
    return false;
}

void QNetCtl::forgetProfile()
{
    QTreeWidgetItem *item = currentItem();
    if (!item)
        return;
    QString profile = item->data(0, ProfileRole).toString();
    if (profile.isEmpty())
        return;
    QMessageBox::StandardButton a = QMessageBox::warning(this, tr("Delete Profile %1 ?").arg(profile),
                                                         tr("Do you really want to delete the profile %1?").arg(profile),
                                                         QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
    if (a == QMessageBox::Yes) {
        emit request("remove_profile", profile);
    }
}

void QNetCtl::readProfiles()
{
    setEnabled(false);
    query(TOOL(netctl) + " list", SLOT(parseProfiles()));
}

void QNetCtl::reply(QString tag, QString information)
{
//     qDebug() << "reply" << tag << information;
    if (information.startsWith("ERROR")) {
        qDebug() << tag << information;
    } else if (tag == "switch_to_profile") {
        readProfiles();
    } else if (tag == "scan_wifi") {
        parseWifiScan(information);
    } else if (tag == "enable_profile") {

    } else if (tag == "enable_service") {

    }
    else if (tag == "disable_profile") {

    } else if (tag == "disable_service") {

    } else if (tag == "remove_profile") {
        readProfiles();
    } else if (tag == "write_profile") {
        readProfiles();
    }
}

void QNetCtl::parseProfiles()
{
    READ_STDOUT(profiles, "Failed to list profiles:");

    QStringList profileList = profiles.split('\n', QString::SkipEmptyParts);
    if (profileList.isEmpty()) {
        setEnabled(true);
        return;
    }
    profiles.clear();
    myProfiles.clear();
    foreach (const QString &profile, profileList) {
        if (profile.startsWith("* ")) {
            myProfiles << Connection(profile.mid(2).trimmed());
            myProfiles.last().active = true;
        } else {
            myProfiles << Connection(profile.trimmed());
        }
    }
    setEnabled(true);
    checkDevices();
    updateTree();
}

void QNetCtl::quitTool()
{
    emit request("quit", "");
}

void QNetCtl::writeProfile(QTreeWidgetItem *item, QString key)
{
    QString name = item->data(0, ProfileRole).toString();
    if (name.isEmpty()) {
        name = item->data(0, SsidRole).toString();
        if (name.isEmpty()) {
            name = item->data(0, InterfaceRole).toString();
        }
        name.prepend("qnetctl-");
    }

    const int type = item->data(0, TypeRole).toInt();
    const bool wireless = type > Connection::Ethernet;
#define DATA(_S_) item->data(0, _S_##Role).toString() + '\n'
    QString profile =   "Description='Written by QNetCtl'\n"
                        "Connection=" + QString(wireless ? "wireless" : "ethernet") + '\n' +
                        "Interface=" + DATA(Interface);
    const QString ip = item->data(0, IPRole).toString();
    if (ip == "dhcp") {
        profile += "IP=dhcp\n"; //‘static’, ‘dhcp’, or ‘no’
//         "IP6" + + //‘static’, ‘stateless’, ‘dhcp-noaddr’, ‘dhcp’, ‘no’ or left out (empty)
    } else {
        QStringList l = ip.split(';');
        if (l.count()) {
            profile += "IP=static\nAddress=" + l.at(0) + '\n'; // An array of IP addresses suffixed with ‘/<netmask>’.
            if (l.count() > 1)                                  // Leaving out brackets for arrays consisting of a single element is accepted in the Bash syntax.
                profile += "Gateway=" + l.at(1) + '\n';
        }
    }

    if (wireless) {
        QString sec;
        if (type > Connection::WEP)
            sec = "wpa"; // TODO ‘wpa-configsection’, or ‘wpa-config’ ?
        else if (type == Connection::WEP)
            sec = "wep";
        else
            sec = "none";
        if (!item->data(0, AutoconnectRole).toBool()) {
            profile += "ExcludeAuto=true\n";
        }
        profile +=  "Security=" + sec + '\n' +
                    "ESSID=" + DATA(Ssid) +
                    "AP=" + DATA(Mac) +// The BSSID (MAC address) of the access point to connect to.
                    "Key=" + key + '\n' +
//                     "Hidden=" + + // Whether or not the specified network is a hidden network. Defaults to ‘no’.
                    "AdHoc=" + QString(item->data(0, AdHocRole).toBool() ? "yes\n" : "no\n");
    }

    emit request("write_profile " + name, profile);
}

void QNetCtl::updateTree()
{
    myUpdateTimer->start();
}

static void map(const Connection &con, QTreeWidgetItem *net)
{
    net->setData(0, IsDetailRole, false);
    net->setData(0, TypeRole, con.type);
    net->setData(0, QualityRole, con.quality);
    net->setData(0, ConnectedRole, con.active);
    net->setData(0, AdHocRole, con.adHoc);
    net->setData(0, ProfileRole, con.profile);
    net->setData(0, InterfaceRole, con.interface);
    net->setData(0, IPRole, con.ipResolution);
    net->setData(0, DescriptionRole, con.description);
    net->setData(0, KeyRole, con.key);
    net->setData(0, MacRole, con.MAC);
    net->setData(0, SsidRole, con.SSID);
    net->setData(0, KeyRole, con.key);
    net->setData(0, AutoconnectRole, con.autoConnect);

    QString title = con.profile;
    if (title.isEmpty()) title = con.SSID;
    if (title.isEmpty()) title = con.MAC;
    if (title.isEmpty()) title = con.interface;
    if (title.isEmpty()) title = "Nameless Network";
    net->setData(0, Qt::DisplayRole, title);
}

void QNetCtl::buildTree()
{
    QList<Connection> temp = myProfiles;
    foreach (const Connection &con, myWLANs) {
        bool required = true;
        for (QList<Connection>::iterator it = temp.begin(), end = temp.end(); it != end; ++it) {
            if ((!con.SSID.isEmpty() && con.SSID == it->SSID) || it->MAC == con.MAC) {
                required = false;
                it->type = con.type;
                it->quality = con.quality;
                it->MAC = con.MAC;
                it->adHoc = con.adHoc;
                break;
            }
        }
        if (required) {
            temp << con;
        }
    }
    for (QMap<QString, bool>::const_iterator it = myDevices.constBegin(),
                                            end = myDevices.constEnd(); it != end; ++it) {
        bool skip = false;
        foreach (const Connection &con, temp) {
            if (con.interface == it.key()) {
                skip = true;
                break;
            }
        }
        if (skip)
            break;
        temp << Connection();
        Connection &con = temp.last();
        con.interface = it.key();
        con.type = (*it) ? Connection::Wireless : Connection::Ethernet;
    }

    int n = myNetworks->invisibleRootItem()->childCount();
    QList<QTreeWidgetItem*> toDelete;
    for (int i = 0; i < n; ++i) {
        QTreeWidgetItem *item = myNetworks->invisibleRootItem()->child(i);
        char p = 0;
        QString match = item->data(0, ProfileRole).toString();
        if (match.isEmpty()) {
            match = item->data(0, SsidRole).toString();
            p = 1;
        }
        if (match.isEmpty()) {
            match = item->data(0, InterfaceRole).toString();
            p = 2;
        }
        if (match.isEmpty())
            p = 3;

        QList<Connection>::iterator it = temp.begin(), end = temp.end();
        while (it != end) {
            const bool matches = (p == 0 && match == it->profile) ||
                                 (p == 1 && match == it->SSID) ||
                                 (p == 2 && match == it->interface);
            if (matches) {
                map(*it, item);
                temp.erase(it); // sic! do not assign it - we break out of the loop and raising it might turn it end
                break;
            }
            ++it;
        }
        if (it == end/* && item->data(0, ProfileRole).toString().isEmpty()*/) {
            // none found, the tree item is not in the available connections -> kick it
            toDelete << item;
        }
    }

    foreach (QTreeWidgetItem *item, toDelete) {
        for (int i = item->childCount() -1; i > -1; --i) {
            delete item->child(i);
        }
        delete item;
    }

    foreach (const Connection &con, temp) {
        QTreeWidgetItem *net = new QTreeWidgetItem;
        map(con, net);
        QTreeWidgetItem *detail = new QTreeWidgetItem(*net);
        detail->setData(0, IsDetailRole, true);
        net->addChild(detail);
        myNetworks->addTopLevelItem(net);
    }

    n = myNetworks->invisibleRootItem()->childCount(); // may have changed
    for (int i = 0; i < n; ++i) {
        QTreeWidgetItem *item = myNetworks->invisibleRootItem()->child(i);
        const int type = item->data(0, TypeRole).toInt();
        const QString interface = item->data(0, InterfaceRole).toString();
        if (type > Connection::Ethernet && myEnabledProfiles.contains("netctl-auto@" + interface + ".service"))
            continue; // controlled by profile attribute
        const bool enabled = (type == Connection::Ethernet && myEnabledProfiles.contains("netctl-ifplugd@" + interface + ".service")) ||
                             myEnabledProfiles.contains(item->data(0, ProfileRole).toString());
        item->setData(0, AutoconnectRole, enabled);
    }
}

void QNetCtl::showSelected(QTreeWidgetItem *item, QTreeWidgetItem *prev)
{
    if (item && item->isExpanded())
        return;
    if (item) {
        myConnectButton->setEnabled(!item->data(0, ConnectedRole).toBool());
        myForgetButton->setEnabled(!item->data(0, ProfileRole).toString().isEmpty());
    }
    if (!item || !item->parent() || item->parent() == myNetworks->invisibleRootItem()) {
        int delay = 0;
        if (prev) {
            if (!prev->isExpanded() && prev->parent() && prev->parent() != item)
                prev = prev->parent();
            if (prev->isExpanded()) {
                delay = 250;
                myNetworks->collapseItem(prev);
            }
        }
        myNetworks->setCurrentItem(item);
        QTimer::singleShot(delay, this, SLOT(expandCurrent()));
    }
}

void QNetCtl::expandCurrent()
{
    myNetworks->expandItem(currentItem());
}

bool QNetCtl::updateAutoConnects()
{
    // determine mode.
    // if there's any wireless profile (usb wlan stick...), autoconnecting eth0 requires ifplugd
    // to act as failover through netctl-ifplugd@interface.service
    // if there's an autoconnecting wireless profile, we'll in addition require wpa_actiond for
    // autoConnect netctl-auto@interface.service
    // if there is only one autoconnecting eth0 profile, we just enable it and disable everything else
    // if there're multiple autoconnecting eth0 profiles, that's for now (TODO: priority??) a conflict
    QStringList autoEth0, autoWifi;
    const int n = myNetworks->invisibleRootItem()->childCount();
    for (int i = 0; i < n; ++i) {
        QTreeWidgetItem *item = myNetworks->invisibleRootItem()->child(i);
        const int type = item->data(0, TypeRole).toInt();
        const bool autoConnect = item->data(0, AutoconnectRole).toBool();
        const QString interface = item->data(0, InterfaceRole).toString();
        if (type > Connection::Ethernet) {
            if (autoConnect && !autoWifi.contains(interface))
                autoWifi << interface;
        } else if (autoConnect) {
            autoEth0 << interface;
        }
    }

    if (autoEth0.removeDuplicates()) {
        QMessageBox::critical(this, tr("Conflictive setup"), tr("You requested to enable several "
                                       "profiles on the same wired interface by default.<br>"
                                       "Please select only one profile per wired interface to "
                                       "automatically connect to.<br><br><b>No changes to autoconnection "
                                       "will be applied unless!</b>"), QMessageBox::Cancel);
        return false;
    }

    // verify that the mode can be used
    bool haveAutoEth0 = !(autoWifi.isEmpty() || autoEth0.isEmpty()),
         haveAutoWLAN = !autoWifi.isEmpty();
    bool needIfPlugD(true), needWpaActionD(true);
    while (needIfPlugD || needWpaActionD) {
        if (haveAutoEth0)
            needIfPlugD = !QFile::exists("/usr/bin/ifplugd"); // autoselect eth0 netctl-ifplugd@interface.service
        else
            needIfPlugD = false;
        if (haveAutoWLAN)
            needWpaActionD = !QFile::exists("/usr/bin/wpa_actiond"); // wifi autoConnect netctl-auto@interface.service
        else
            needWpaActionD = false;
        if (needIfPlugD || needWpaActionD) {
            QString message;
            if (needIfPlugD) {
                message = tr("You have chosen to automatically activate<br>a wired connection when it is "
                             "available, but you lack<br>the package <b>ifplug</b> for this purpose.<br>"
                             "Please install it or choose to activate all wired connections manually.");
                if (needWpaActionD)
                    message += "<br><br>";
            }
            if (needWpaActionD) {
                message += tr("You have chosen to automatically activate<br>a wireless connection when it is "
                              "available, but you lack<br>the package <b>wpa_actiond</b> for this purpose.<br>"
                              "Please install it or choose to activate all wireless connections manually.");
            }
            if (QMessageBox::critical(this, tr("Please install required packages"), message,
                                      QMessageBox::Abort|QMessageBox::Retry, QMessageBox::Retry) == QMessageBox::Abort) {
                QMessageBox::critical(this, tr("All automatic connections will be enabled unconditionally"),
                                            tr("You have chosen to automatically activate multiple connections, "
                                               "but lack the tools to select the best one automatically.<br>"
                                               "Therefore <b>all connections will automatically be enabled simultaniously!</b><br>"
                                               "Reconsider the setup"), QMessageBox::Ok);
                return false;
            }
        }
    }

    haveAutoEth0 |= !needIfPlugD;
    haveAutoWLAN |= !needWpaActionD;

    // now en/disable profiles
    QStringList requiredProfiles;
    if (haveAutoEth0 || haveAutoWLAN) {
        if (haveAutoEth0) {
            foreach (const QString &interface, autoEth0) {
                const QString profile("netctl-ifplugd@" + interface + ".service");
                requiredProfiles << profile;
                myEnabledProfiles.removeAll(profile);
            }
        }
        if (haveAutoWLAN) {
            foreach (const QString &interface, autoWifi) {
                const QString profile("netctl-auto@" + interface + ".service");
                requiredProfiles << profile;
                myEnabledProfiles.removeAll(profile);
            }
        }
    } else { // simple eth0 setup
        for (int i = 0; i < n; ++i) {
            QTreeWidgetItem *item = myNetworks->invisibleRootItem()->child(i);
            if (item->data(0, AutoconnectRole).toBool()) {
                const QString profile(item->data(0, ProfileRole).toString());
                requiredProfiles << profile;
                myEnabledProfiles.removeAll(profile);
            }
        }
    }

    // disable remaining enabled ones
    foreach (const QString &profile, myEnabledProfiles) {
        // profiles ending with .service are illegal and meant for systemctl
        emit request(profile.endsWith(".service") ? "disable_service" : "disable_profile", profile);
    }

    myEnabledProfiles = requiredProfiles;
    // enable required
    foreach (const QString &profile, myEnabledProfiles) {
        // profiles ending with .service are illegal and meant for systemctl
        emit request(profile.endsWith(".service") ? "enable_service" : "enable_profile", profile);
    }

    return true;
}

void QNetCtl::verifyPath()
{
    QLineEdit *le = static_cast<QLineEdit*>(sender());
    if (le->text().isEmpty() || QFile::exists(le->text().section(" ", 0, 0)))
        le->setPalette(QPalette());
    else {
        QPalette pal(le->palette());
        pal.setColor(le->foregroundRole(), Qt::red);
        le->setPalette(pal);
    }
}

static QNetCtl *gs_netCtl = 0;

void signalHandler(int signal)
{
    if (!gs_netCtl)
        return;
    if (signal == SIGTERM || signal == SIGQUIT || signal == SIGINT) {
        gs_netCtl->quitTool();
        QApplication::quit();
    } else if (signal == SIGSEGV || signal == SIGABRT) {
        gs_netCtl->quitTool();
    }
}

int main(int argc, char **argv)
{
    signal(SIGSEGV, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGQUIT, signalHandler);
    signal(SIGINT, signalHandler);
    signal(SIGABRT, signalHandler);

    QApplication a(argc, argv);
    gs_netCtl = new QNetCtl;
    gs_netCtl->show();
    return a.exec();
}