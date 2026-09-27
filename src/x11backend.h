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

#ifndef X11BACKEND_H
#define X11BACKEND_H

#include "kblayoutbackend.h"
#include <QHash>

class QAbstractNativeEventFilter;

namespace pimpl {
    class NativeEventFilter;
}

/**
 * \brief X11 backend using libxkbcommon-x11 + XCB.
 *
 * Reads layout from X server via xkb_x11_*, switches layouts via
 * xcb_xkb_latch_lock_state(), and listens for XKB state changes
 * via native event filter.
 */
class X11Backend : public KbLayoutBackend
{
    Q_OBJECT
public:
    explicit X11Backend(QObject *parent = nullptr);
    ~X11Backend() override;

    bool isValid() const override { return m_valid; }
    QStringList layouts() const override;
    int currentLayout() const override;
    void setLayout(int index) override;
    void nextLayout() override;

signals:
    /**
     * \brief Emitted when LED state changes (Caps/Num/Scroll).
     */
    void ledStateChanged(bool caps, bool num, bool scroll);

public:
    /**
     * \brief Accessors for internal X11 state.
     */
    void *connection() const { return m_connection; }
    int deviceId() const { return m_deviceId; }

private:
    friend class pimpl::NativeEventFilter;
    bool init();
    void readState();
    void readLedState();
    void readKbdInfo();

    bool m_valid{false};
    void *m_context{nullptr};
    void *m_connection{nullptr};
    int m_deviceId{0};
    uint8_t m_eventType{0};
    void *m_state{nullptr};
    void *m_keymap{nullptr};
    QStringList m_layoutSyms;
    QStringList m_layoutNames;
    QHash<QString, QString> m_langCache;
    QAbstractNativeEventFilter *m_eventFilter{nullptr};

    /**
     * \brief Parse /usr/share/X11/xkb/rules/evdev.xml and cache layout/variant names.
     */
    void parseEvdevXml();
};

#endif // X11BACKEND_H
