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

#ifndef KBLAYOUT_H
#define KBLAYOUT_H

#include <lxqt/ilxqtpanelplugin.h>
#include "kblayoutwidget.h"
#include "kblayoutbackend.h"
#include "kblayoutsettings.h"

#ifdef KBLAYOUT_X11
#include "kbdkeeper.h"
#endif

class QDialog;
class QTranslator;

/**
 * \brief Main LXQt panel plugin for keyboard layout switching.
 *
 * Provides a clickable widget on the panel that shows the current keyboard
 * layout and allows switching between layouts via left/right click.
 * Supports both X11 (via libxkbcommon-x11) and Wayland (via compositor IPC).
 */
class KbLayout : public QObject, public ILXQtPanelPlugin
{
    Q_OBJECT
public:
    KbLayout(const ILXQtPanelPluginStartupInfo &startupInfo);
    ~KbLayout() override;

    virtual QString themeId() const override
    { return QStringLiteral("KbLayout"); }

    virtual ILXQtPanelPlugin::Flags flags() const override
    { return PreferRightAlignment | HaveConfigDialog; }

    virtual QWidget *widget() override
    { return &m_widget; }

    QDialog *configureDialog() override;
    virtual void realign() override;

protected slots:
    virtual void settingsChanged() override;
    void activated(ActivationReason reason) override;

private slots:
    // Widget signal handlers
    void onLayoutNextRequested();
    void onLayoutSelected(int index);
    void onConfigureRequested();

private:
    void createBackend();
    void updateWidgetFromBackend();
    void showLayoutNotification(const QString &layoutName);
    void updateLedState(bool caps, bool num, bool scroll);

    KbLayoutSettings m_settings;
    KbLayoutBackend *m_backend{nullptr};
    KbLayoutWidget  m_widget;
    QTranslator *m_translator{nullptr};
#ifdef KBLAYOUT_X11
    KbLayoutKeeper *m_keeper{nullptr};
#endif
    int m_lastLayoutIdx{-1}; // Track changes for notifications
};

#endif // KBLAYOUT_H
