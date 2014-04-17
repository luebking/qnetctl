HEADERS     = QNetCtl.h QNetCtl_dbus.h
SOURCES     = QNetCtl.cpp
FORMS       = ipconfig.ui settings.ui
QT          += dbus
TARGET      = qnetctl
VERSION     = 0.1
target.path += $$[QT_INSTALL_BINS]
INSTALLS    += target
