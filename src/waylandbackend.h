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

#ifndef WAYLANDBACKEND_H
#define WAYLANDBACKEND_H

#include "kblayoutbackend.h"

class QDBusInterface;
class QTimer;

/**
 * \brief Wayland backend using compositor-specific IPC.
 *
 * Supported compositors:
 * - KWin Wayland: D-Bus org.kde.KeyboardLayouts (full support)
 * - Sway: swaymsg JSON IPC + polling (full support)
 * - Hyprland: hyprctl JSON IPC + polling (full support)
 * - Labwc: wtype key emulation (limited — read-only detection)
 * - Wayfire: wtype key emulation (limited — read-only detection)
 * - Generic fallback: read-only with "us" layout
 */
class WaylandBackend : public KbLayoutBackend
{
    Q_OBJECT
public:
    explicit WaylandBackend(QObject *parent = nullptr);
    ~WaylandBackend() override;

    bool isValid() const override { return m_valid; }
    QStringList layouts() const override;
    int currentLayout() const override;
    void setLayout(int index) override;
    void nextLayout() override;

private:
    /**
     * \brief Detect compositor and initialize appropriate backend.
     */
    bool init();

    /**
     * \brief KWin Wayland via D-Bus.
     */
    bool initKWin();
    void readKWinLayouts();
    void readKXkbConfig();

    /**
     * \brief Sway via swaymsg JSON IPC.
     */
    bool initSway();
    void readSwayLayouts();
    void readSwayConfigLayouts();

    /**
     * \brief Hyprland via hyprctl JSON IPC.
     */
    bool initHyprland();
    void readHyprlandLayouts();

    /**
     * \brief Labwc — key emulation fallback.
     */
    bool initLabwc();

    /**
     * \brief Wayfire — key emulation fallback.
     */
    bool initWayfire();

    /**
     * \brief Generic fallback for unknown compositors.
     */
    bool initGeneric();

    /**
     * \brief Parse swaymsg JSON output to extract layout info.
     */
    void parseSwayJson(const QString &json);

    /**
     * \brief Fallback: switch layout by emulating Alt+Shift key press.
     * Uses wtype (preferred on Wayland) or xdotool.
     */
    void fallbackSwitchViaKeyEmulation();

    /**
     * \brief Polling timer for compositors without D-Bus signals.
     */
    QTimer *m_pollTimer{nullptr};

    /**
     * \brief Polling timer for LED state detection.
     */
    QTimer *m_ledPollTimer{nullptr};

    /**
     * \brief Current active layout name (from polling).
     */
    QString m_activeLayoutName;

    /**
     * \brief Current layout index (for polling-based compositors).
     */
    int m_currentIdx{0};

    /**
     * \brief Polling interval in milliseconds.
     */
    int m_pollInterval{2000};

    /**
     * \brief Cached LED states.
     */
    bool m_ledCaps{false};
    bool m_ledNum{false};
    bool m_ledScroll{false};

private slots:
    /**
     * \brief D-Bus slot for KWin layoutChanged signal.
     */
    void _on_kwin_layoutChanged(uint index);

    /**
     * \brief Polling timer slot for Sway/Hyprland layout detection.
     */
    void _on_poll_timer();

    /**
     * \brief Polling timer slot for LED state detection.
     */
    void _on_led_poll_timer();

    /**
     * \brief Read LED states from sysfs.
     */
    void readLedStatesFromXkb();

    /**
     * \brief Emit initial LED state to widget.
     */
    void emitInitialLedState();

private:
    bool m_valid{false};
    QString m_compositor;
    QStringList m_layoutSyms;
    QStringList m_layoutNames;

    // KWin D-Bus
    QDBusInterface *m_kwinLayouts{nullptr};

    /**
     * \brief Cached current layout index (from D-Bus signal).
     */
    int m_cachedLayoutIdx{0};
};

#endif // WAYLANDBACKEND_H
