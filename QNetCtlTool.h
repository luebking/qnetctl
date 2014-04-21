#ifndef QNETCTLTOOL_H
#define QNETCTLTOOL_H

#include <QCoreApplication>
#include <QStringList>

class QDBusInterface;

class QNetCtlTool : public QCoreApplication
{
    Q_OBJECT
public:
    QNetCtlTool(int &argc, char **argv);
private slots:
    void chain();
    void scanWifi(QString device = QString());
    void reply();
    void request(QString tag, QString information);
private:
    QDBusInterface *myClient;
    QStringList myScanningDevices, myUplinkingDevices;
};

#endif // QNETCTLTOOL_H