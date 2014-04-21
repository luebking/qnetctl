
#include "QNetCtlTool.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>

#include <unistd.h>

#include "paths.h"

static void debug(QString s) {
    QFile file("/tmp/qnetctl.dbg");
    file.open(QIODevice::Append);
    file.write(s.append("\n").toLocal8Bit());
}

QNetCtlTool::QNetCtlTool(int &argc, char **argv) : QCoreApplication(argc, argv)
{
    if (argc < 4) {
        qWarning("Must pass clients DBus address, name and service!");
        QMetaObject::invokeMethod(this, "quit", Qt::QueuedConnection);
        return;
    }
    seteuid(1000);
    QDBusConnection bus = QDBusConnection::connectToBus(argv[1], argv[2]);
    seteuid(0);

    myClient = new QDBusInterface(argv[3], "/QNetCtl", "org.archlinux.qnetctl", bus, this);
    bus.connect(argv[3], "/QNetCtl", "org.archlinux.qnetctl", "request", this, SLOT(request(QString, QString)));
}


void QNetCtlTool::chain()
{
    QProcess *proc = static_cast<QProcess*>(sender());
    const QString tag = proc->property("QNetCtlTag").toString();
    const QString info = proc->property("QNetCtlInfo").toString();
    if (proc->exitStatus() != QProcess::NormalExit || proc->exitCode()) {
        myClient->call(QDBus::NoBlock, "reply", tag, QString("ERROR: %1, %2").arg(proc->exitStatus()).arg(proc->exitCode()));
        return;
    }
    myClient->call(QDBus::NoBlock, "reply", tag, QString::fromLocal8Bit(proc->readAllStandardOutput()));

    QString cmd;
    if (tag == "remove_profile") {
        QFile::remove(gs_profilePath + info);
    } else if (tag == "scan_wifi") {
        myScanningDevices.removeAll(info);
        myUplinkingDevices.removeAll(info);
        cmd = TOOL(ip) + " link set " + info + " down";
    }

    if (cmd.isNull()) // should not happen
        return;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove("LC_ALL");
    env.remove("LANG");
    proc = new QProcess(this);
    proc->setProcessEnvironment(env);
//     proc->setProperty("QNetCtlTag", tag);
//     connect (proc, SIGNAL(finished(int, QProcess::ExitStatus)), SLOT(reply()));
    connect (proc, SIGNAL(finished(int, QProcess::ExitStatus)), proc, SLOT(deleteLater()));
    proc->start(cmd, QIODevice::ReadOnly);
}

void QNetCtlTool::scanWifi(QString device)
{
    if (device.isNull() && sender())
        device = sender()->property("QNetCtlScanDevice").toString();

    if (myScanningDevices.contains(device))
        return;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove("LC_ALL");
    env.remove("LANG");
    QProcess *proc = new QProcess(this);
    proc->setProcessEnvironment(env);

    bool isDown = false;
    proc->start(TOOL(ip) + " link show " + device, QIODevice::ReadOnly);
    proc->waitForFinished();
    if (proc->exitStatus() == QProcess::NormalExit && !proc->exitCode())
        isDown = !QString::fromLocal8Bit(proc->readAllStandardOutput()).section('>', 0, 0).contains("UP");

    bool waitsForUp = myUplinkingDevices.contains(device);

    if (isDown) {
        if (!waitsForUp) {
            waitsForUp = true;
            myUplinkingDevices << device;
            proc->start(TOOL(ip) + " link set " + device + " up", QIODevice::ReadOnly);
            proc->waitForFinished();
        }

        // we're waiting for the device to come up
        delete proc;
        QTimer *t = new QTimer(this);
        t->setProperty("QNetCtlScanDevice", device);
        t->setSingleShot(true);
        connect(t, SIGNAL(timeout()), this, SLOT(scanWifi()));
        connect(t, SIGNAL(timeout()), t, SLOT(deleteLater()));
        t->start(500);
        return;
    }

    myScanningDevices << device;

    proc->setProperty("QNetCtlTag", "scan_wifi");
    proc->setProperty("QNetCtlInfo", device);
    // if we set it up, we've to set it back down through the chain slot
    connect(proc, SIGNAL(finished(int, QProcess::ExitStatus)), waitsForUp ? SLOT(chain()) : SLOT(reply()));
    connect(proc, SIGNAL(finished(int, QProcess::ExitStatus)), proc, SLOT(deleteLater()));
    proc->start(TOOL(iw) + " dev " + device + " scan");
}

void QNetCtlTool::reply()
{
    QProcess *proc = static_cast<QProcess*>(sender());
    const QString tag = proc->property("QNetCtlTag").toString();
    if (proc->exitStatus() != QProcess::NormalExit || proc->exitCode()) {
        myClient->call(QDBus::NoBlock, "reply", tag, QString("ERROR: %1, %2").arg(proc->exitStatus()).arg(proc->exitCode()));
        return;
    }
    myClient->call(QDBus::NoBlock, "reply", tag, QString::fromLocal8Bit(proc->readAllStandardOutput()));
}

void QNetCtlTool::request(const QString tag, const QString information)
{
    QString cmd;
    bool chain = false;
//     debug(tag + information);
    if (tag == "switch_to_profile") {
        cmd = TOOL(netctl) + " switch-to " + information;
    } else if (tag == "stop_profile") {
        cmd = TOOL(netctl) + " stop " + information;
    } else if (tag == "scan_wifi") {
        scanWifi(information);
        return;
    } else if (tag == "enable_profile") {
        cmd = TOOL(netctl) + " enable " + information;
    } else if (tag == "enable_service") {
        if (information.startsWith("netctl-"))
            cmd = TOOL(systemctl) + " enable " + information;
    }
    else if (tag == "disable_profile") {
        cmd = TOOL(netctl) + " disable " + information;
    } else if (tag == "disable_service") {
        if (information.startsWith("netctl-"))
            cmd = TOOL(systemctl) + " disable " + information;
    } else if (tag == "remove_profile") {
        chain = true;
        cmd = TOOL(netctl) + " disable " + information;
    } else if (tag.startsWith("write_profile")) {
        QString name = tag.section(' ', 1);
        QFile file(gs_profilePath + name);
        if (file.open(QIODevice::WriteOnly|QIODevice::Text)) {
            file.write(information.toLocal8Bit());
            file.close();
            myClient->call(QDBus::NoBlock, "reply", tag, "SUCCESS");
        } else {
            myClient->call(QDBus::NoBlock, "reply", tag, "ERROR");
        }
        return; // no process to run
    } else if (tag == "reparse_config") {
        return;
    } else if (tag == "quit") {
        quit();
        return;
    }

    if (cmd.isNull()) {
        myClient->call(QDBus::NoBlock, "reply", tag, "ERROR: unsupported command / request:" + information);
        return;
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove("LC_ALL");
    env.remove("LANG");
    QProcess *proc = new QProcess(this);
    proc->setProcessEnvironment(env);
    proc->setProperty("QNetCtlTag", tag);
    if (chain) {
        proc->setProperty("QNetCtlInfo", information);
        connect (proc, SIGNAL(finished(int, QProcess::ExitStatus)), SLOT(chain()));
    } else {
        connect (proc, SIGNAL(finished(int, QProcess::ExitStatus)), SLOT(reply()));
    }
    connect (proc, SIGNAL(finished(int, QProcess::ExitStatus)), proc, SLOT(deleteLater()));
//     debug(cmd);
    proc->start(cmd, QIODevice::ReadOnly);
}

int main(int argc, char **argv)
{
    return QNetCtlTool(argc, argv).exec();
}