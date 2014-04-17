
#include "QNetCtlTool.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>

#include <unistd.h>

#include "paths.h"

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
    QFile file("/tmp/qnetctl.dbg");
    file.open(QIODevice::Append);
    file.write(bus.isConnected() ? "connected" : "error!");
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

    QString cmd;
    if (tag == "remove_profile") {
        QFile::remove(gs_profilePath + info);
    } else if (tag == "scan_wifi") {
        cmd = TOOL(ip) + " link set " + info + " down";
    }

    if (cmd.isNull()) // should not happen
        return;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove("LC_ALL");
    env.remove("LANG");
    proc = new QProcess(this);
    proc->setProcessEnvironment(env);
    proc->setProperty("QNetCtlTag", tag);
    connect (proc, SIGNAL(finished(int, QProcess::ExitStatus)), SLOT(reply()));
    connect (proc, SIGNAL(finished(int, QProcess::ExitStatus)), proc, SLOT(deleteLater()));
    proc->start(cmd, QIODevice::ReadOnly);
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
    if (tag == "switch_to_profile") {
        cmd = TOOL(netctl) + " switch-to " + information;
    } else if (tag == "scan_wifi") {
        bool wasDown = false;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove("LC_ALL");
        env.remove("LANG");
        QProcess proc2(this);
        proc2.setProcessEnvironment(env);
        proc2.start(TOOL(ip) + " link show " + information, QIODevice::ReadOnly);
        proc2.waitForFinished();
        if (proc2.exitStatus() == QProcess::NormalExit && !proc2.exitCode())
            wasDown = QString::fromLocal8Bit(proc2.readAllStandardOutput()).contains("state DOWN");
        if (wasDown) {
            chain = true;
            proc2.start(TOOL(ip) + " link set " + information + " up", QIODevice::ReadOnly);
            proc2.waitForFinished();
        }
        cmd = TOOL(iw) + " dev " + information + " scan";
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

    } else if (tag == "quit") {
        quit();
        return;
    }

    if (cmd.isNull()) {
        myClient->call(QDBus::NoBlock, "reply", tag, "ERROR: unsupported command / request:" + information);
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
    proc->start(cmd, QIODevice::ReadOnly);
}

int main(int argc, char **argv)
{
    return QNetCtlTool(argc, argv).exec();
}