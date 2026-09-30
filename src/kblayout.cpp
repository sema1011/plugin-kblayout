/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * LXQt - a lightweight, Qt based, desktop toolset
 * https://lxqt.org
 *
 * Copyright: 2026 LXQt team
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "kblayout.h"
#include "kblayoutsettings.h"
#include "kblayoutbackend.h"
#include "kblayoutwidget.h"
#include "kblayoutsettingsdialog.h"
#include "kblayout-config.h"

#include <QTranslator>
#include <QApplication>
#include <QLocale>
#include <QDir>

#ifdef KBLAYOUT_X11
#include "x11backend.h"
#endif

#ifdef KBLAYOUT_WAYLAND
#include "waylandbackend.h"
#endif

#include <QTimer>
#include <QGuiApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

#ifdef KBLAYOUT_X11
#include <xcb/xcb.h>
#endif

#include <LXQt/lxqtsettings.h>

KbLayout::KbLayout(const ILXQtPanelPluginStartupInfo &startupInfo) :
    QObject(),
    ILXQtPanelPlugin(startupInfo),
    m_widget()
{
    // Install translator for plugin localization
    QString locale = QLocale::system().name();  // e.g. "ru_RU", "en_US"
    QString translationPath = LXQT_SHARE_DIR "/translations/";
    
    // Try locale-specific translation (e.g. ru_RU -> ru.qm)
    QString qmFile = locale.split('_').first();  // "ru"
    if (m_translator.load(qmFile, translationPath)) {
        QApplication::installTranslator(&m_translator);
    }
    m_settings.init(settings());

    // Set panel reference for popup positioning
    if (panel()) {
        m_widget.setPanel(panel());
    }

    createBackend();

    // Connect backend signals to widget slots
    if (m_backend) {
        connect(m_backend, &KbLayoutBackend::layoutChanged,
                this, &KbLayout::updateWidgetFromBackend);

        // Connect LED state signal (works for both X11 and Wayland)
        connect(m_backend, &KbLayoutBackend::ledStateChanged,
                this, &KbLayout::updateLedState);

        // Create KbdKeeper for X11
#ifdef KBLAYOUT_X11
        auto *x11Backend = qobject_cast<X11Backend*>(m_backend);
        if (x11Backend) {
            m_keeper = new WinKbdKeeper(
                static_cast<xcb_connection_t*>(x11Backend->connection()),
                x11Backend->deviceId());
            m_keeper->setNumGroups(x11Backend->numGroups());
            m_keeper->setup();
        }
#endif
    }

    // Connect widget signals to plugin slots
    connect(&m_widget, &KbLayoutWidget::layoutNextRequested,
            this, &KbLayout::onLayoutNextRequested);
    connect(&m_widget, &KbLayoutWidget::layoutSelected,
            this, &KbLayout::onLayoutSelected);
    connect(&m_widget, &KbLayoutWidget::configureRequested,
            this, &KbLayout::onConfigureRequested);

    settingsChanged();

    // Emit initial LED state AFTER settings are applied
    if (m_backend) {
        m_backend->emitInitialLedState();
    }
}

KbLayout::~KbLayout()
{
#ifdef KBLAYOUT_X11
    if (m_keeper)
        delete m_keeper;
#endif
}

void KbLayout::createBackend()
{
    QString platform = QGuiApplication::platformName();
    QString sessionType = qEnvironmentVariable("XDG_SESSION_TYPE");

#ifdef KBLAYOUT_WAYLAND
    if (platform == QLatin1String("wayland") || sessionType == QLatin1String("wayland")) {
        auto *backend = new WaylandBackend(this);
        if (backend->isValid()) {
            m_backend = backend;
        } else {
            delete backend;
        }
    }
#endif

    if (!m_backend) {
#ifdef KBLAYOUT_X11
        if (platform == QLatin1String("xcb") || sessionType == QLatin1String("x11") || sessionType.isEmpty()) {
            auto *x11App = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
            if (x11App && x11App->connection()) {
                auto *backend = new X11Backend(this);
                if (backend->isValid()) {
                    m_backend = backend;
                } else {
                    delete backend;
                }
            }
        }
#endif
    }

    if (!m_backend) {
        qWarning() << "kblayout: No backend available for platform"
                    << platform << "session type" << sessionType;
    }
}

void KbLayout::updateWidgetFromBackend()
{
    if (!m_backend)
        return;

    int newIdx = m_backend->currentLayout();
    const auto &syms = m_backend->layouts();
    const auto &names = m_backend->layoutNames();

    m_widget.setLayouts(syms, names);
    m_widget.setCurrentLayout(newIdx);

    // Show notification if layout actually changed
    if (newIdx != m_lastLayoutIdx && m_settings.showNotification()) {
        QString layoutName;
        if (newIdx >= 0 && newIdx < syms.size()) {
            if (!names.isEmpty() && newIdx < names.size()) {
                layoutName = names[newIdx];
            } else {
                layoutName = syms[newIdx].toUpper();
            }
        }
        showLayoutNotification(layoutName);
    }
    m_lastLayoutIdx = newIdx;
}

QDialog *KbLayout::configureDialog()
{
    return new KbLayoutSettingsDialog(&m_settings, nullptr);
}

void KbLayout::realign()
{
    if (panel()->isHorizontal()) {
        m_widget.setMinimumSize(0, panel()->iconSize());
    } else {
        m_widget.setMinimumSize(panel()->iconSize(), 0);
    }
}

void KbLayout::settingsChanged()
{
    // Apply display settings
    m_widget.setShowCaps(m_settings.showCaps());
    m_widget.setShowNum(m_settings.showNum());
    m_widget.setShowScroll(m_settings.showScroll());
    m_widget.setShowFlags(m_settings.showFlags());
    m_widget.setFontSize(m_settings.fontSize());
    m_widget.setFlagPattern(m_settings.flagPattern());

    m_widget.setup();
    updateWidgetFromBackend();

    // Emit initial LED state after settings are applied
    if (m_backend) {
        m_backend->emitInitialLedState();
    }
}

void KbLayout::updateLedState(bool caps, bool num, bool scroll)
{
    m_widget.setLedState(caps, num, scroll);
}

void KbLayout::activated(ActivationReason reason)
{
    if (reason == Trigger && m_backend) {
        m_backend->nextLayout();
    }
}

// ============================================================================
// Widget signal handlers
// ============================================================================

void KbLayout::onLayoutNextRequested()
{
    if (m_backend) {
        m_backend->nextLayout();
    }
}

void KbLayout::onLayoutSelected(int index)
{
    if (m_backend) {
        m_backend->setLayout(index);
    }
}

void KbLayout::onConfigureRequested()
{
    QDialog *dlg = configureDialog();
    if (dlg) {
        dlg->open();
    }
}

// ============================================================================
// Notification
// ============================================================================

void KbLayout::showLayoutNotification(const QString &layoutName)
{
    if (layoutName.isEmpty())
        return;

    // Try KDE Plasma notification (KNotify)
    QDBusInterface notify(QStringLiteral("org.kde.KNotify"),
                           QStringLiteral("/Notify"),
                           QStringLiteral("org.kde.KNotify"),
                           QDBusConnection::sessionBus());

    if (notify.isValid()) {
        QDBusReply<uint> reply = notify.call(QStringLiteral("event"),
                                              QStringLiteral("Keyboard Layout"),
                                              layoutName,
                                              QString(),
                                              QString(),
                                              QString(),
                                              QString(),
                                               0,
                                               Kblayout::NotificationTimeoutMs);
        if (reply.isValid()) {
            return;
        }
    }

    // Fallback: FreeDesktop notification (Notify OSD / dunst)
    QDBusInterface notify2(QStringLiteral("org.freedesktop.Notifications"),
                            QStringLiteral("/org/freedesktop/Notifications"),
                            QStringLiteral("org.freedesktop.Notifications"),
                            QDBusConnection::sessionBus());

    if (notify2.isValid()) {
        // Arguments: app_name, replaces_id, app_icon, summary, body, actions, hints, timeout
        QDBusMessage msg = QDBusMessage::createMethodCall(
            notify2.service(), notify2.path(), notify2.interface(), QStringLiteral("Notify"));
        msg << QStringLiteral("LXQt Keyboard Layout")  // app_name
            << uint(0)                                  // replaces_id
            << QStringLiteral("input-keyboard")         // app_icon
            << tr("Layout changed")                     // summary
            << layoutName                              // body
            << QStringList()                           // actions
            << QVariantMap()                           // hints
            << Kblayout::ProcessTimeoutMs;              // timeout

        QDBusConnection::sessionBus().send(msg);
    }
}
