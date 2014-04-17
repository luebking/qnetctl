#ifndef QNETCTL_PATHS_H
#define QNETCTL_PATHS_H

#include <QString>

static QString gs_profilePath("/etc/netctl/");
static const struct {
    QString ip, iw, netctl, qnetctl, rfkill, systemctl;
} tools = { "/usr/bin/ip", "/usr/bin/iw", "/usr/bin/netctl", "/usr/bin/qnetctl_tool", "/usr/bin/rfkill", "/usr/bin/systemctl" };

#define TOOL(_T_) tools._T_

#endif // QNETCTL_PATHS_H