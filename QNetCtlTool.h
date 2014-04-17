#ifndef QNETCTLTOOL_H
#define QNETCTLTOOL_H

#include <QCoreApplication>

class QDBusInterface;

class QNetCtlTool : public QCoreApplication
{
    Q_OBJECT
public:
    QNetCtlTool(int &argc, char **argv);
private slots:
    void chain();
    void reply();
    void request(QString tag, QString information);
private:
    QDBusInterface *myClient;
};

#endif // QNETCTLTOOL_H