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

#ifndef KBDKEEPER_H
#define KBDKEEPER_H

#include <QObject>
#include <QHash>
#include <QTimer>
#include <xcb/xcb.h>

/**
 * \brief Keeper types for keyboard layout persistence.
 */
enum class KeeperType
{
    Global,   ///< Keep layout globally
    Window,   ///< Keep layout per-window
    Application ///< Keep layout per-application
};

/**
 * \brief Abstract base class for keyboard layout keeper.
 *
 * KbdKeeper remembers which layout was active for each window/application
 * and restores it when the window/application gains focus.
 */
class KbLayoutKeeper : public QObject
{
    Q_OBJECT
public:
    explicit KbLayoutKeeper(xcb_connection_t *conn, int deviceId, KeeperType type = KeeperType::Global);
    virtual ~KbLayoutKeeper() = default;

    /**
     * \brief Initialize the keeper.
     */
    virtual bool setup();

    /**
     * \brief Switch to the next layout.
     */
    void switchToNext();

    /**
     * \brief Switch to a specific group (layout).
     * \param group Group index.
     */
    virtual void switchToGroup(uint group);

    KeeperType type() const { return m_type; }

signals:
    /**
     * \brief Emitted when the layout changes.
     */
    void changed();

protected:
    /**
     * \brief Called when keyboard configuration changes.
     */
    virtual void keyboardChanged();

    /**
     * \brief Called when layout changes.
     * \param group New group index.
     */
    virtual void layoutChanged(uint group);

    /**
     * \brief Periodically check and restore layout state.
     */
    virtual void checkState();

    xcb_connection_t *m_conn{nullptr};
    int m_deviceId{0};
    KeeperType m_type;
    QTimer *m_timer{nullptr};
    int m_currentGroup{0};
};

/**
 * \brief Window-level layout keeper.
 *
 * Remembers layout per-window (by X11 window ID) and restores it
 * when the window gains focus.
 */
class WinKbdKeeper : public KbLayoutKeeper
{
    Q_OBJECT
public:
    explicit WinKbdKeeper(xcb_connection_t *conn, int deviceId);
    ~WinKbdKeeper() override = default;

    void switchToGroup(uint group) override;

protected:
    void layoutChanged(uint group) override;
    void checkState() override;

private:
    QHash<xcb_window_t, int> m_mapping;
    xcb_window_t m_activeWindow{0};
};

/**
 * \brief Application-level layout keeper.
 *
 * Remembers layout per-application (by window class) and restores it
 * when an application window gains focus.
 */
class AppKbdKeeper : public KbLayoutKeeper
{
    Q_OBJECT
public:
    explicit AppKbdKeeper(xcb_connection_t *conn, int deviceId);
    ~AppKbdKeeper() override = default;

    void switchToGroup(uint group) override;

protected:
    void layoutChanged(uint group) override;
    void checkState() override;

private:
    QHash<QString, int> m_mapping;
    QString m_activeClass;
};

#endif // KBDKEEPER_H
