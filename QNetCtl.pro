HEADERS     = QNetCtl.h QNetCtl_dbus.h
SOURCES     = QNetCtl.cpp
FORMS       = ipconfig.ui settings.ui
QT          += dbus widgets
TARGET      = qnetctl
VERSION     = 0.1
target.path += /usr/bin
INSTALLS    += target
