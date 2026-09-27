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

#include "kbdkeeper.h"

#include <xcb/xkb.h>
#include <xcb/xproto.h>
#include <xcb/xfixes.h>

KbLayoutKeeper::KbLayoutKeeper(xcb_connection_t *conn, int deviceId, KeeperType type) :
    m_conn(conn),
    m_deviceId(deviceId),
    m_type(type)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(500); // Check every 500ms
    connect(m_timer, &QTimer::timeout, this, &KbLayoutKeeper::checkState);
}

bool KbLayoutKeeper::setup()
{
    m_timer->start();
    return true;
}

void KbLayoutKeeper::switchToNext()
{
    // Cycle to next group
    switchToGroup((m_currentGroup + 1) % 4); // Assume max 4 groups
}

void KbLayoutKeeper::switchToGroup(uint group)
{
    if (!m_conn || m_deviceId < 0)
        return;

    xcb_void_cookie_t cookie = xcb_xkb_latch_lock_state(
        m_conn,
        static_cast<xcb_xkb_device_spec_t>(m_deviceId),
        0, 0, // affectMods, latchedMods
        1, static_cast<uint8_t>(group), // lockedMods, group
        0, 0, // keys, otherState
        0); // otherLatchedMods

    xcb_generic_error_t *error = xcb_request_check(m_conn, cookie);
    if (error) {
        free(error);
        return;
    }

    m_currentGroup = group;
    emit changed();
}

void KbLayoutKeeper::keyboardChanged()
{
    // Reset mapping on keyboard change
    m_mapping.clear();
    m_activeWindow = 0;
    m_activeClass.clear();
}

void KbLayoutKeeper::layoutChanged(uint group)
{
    m_currentGroup = group;
    emit changed();
}

void KbLayoutKeeper::checkState()
{
    // Override in subclasses
}

// ============================================================================
// WinKbdKeeper
// ============================================================================

WinKbdKeeper::WinKbdKeeper(xcb_connection_t *conn, int deviceId) :
    KbLayoutKeeper(conn, deviceId, KeeperType::Window)
{
}

void WinKbdKeeper::switchToGroup(uint group)
{
    // Remember current window's layout
    if (m_activeWindow) {
        m_mapping[m_activeWindow] = m_currentGroup;
    }

    KbLayoutKeeper::switchToGroup(group);

    // Update active window
    xcb_window_t focus;
    int revert;
    xcb_get_input_focus_reply_t *reply = xcb_get_input_focus_reply(
        m_conn,
        xcb_get_input_focus(m_conn),
        nullptr);

    if (reply) {
        m_activeWindow = reply->focus;
        free(reply);
    }
}

void WinKbdKeeper::layoutChanged(uint group)
{
    KbLayoutKeeper::layoutChanged(group);

    // Restore layout for current window
    if (m_activeWindow && m_mapping.contains(m_activeWindow)) {
        switchToGroup(m_mapping[m_activeWindow]);
    }
}

void WinKbdKeeper::checkState()
{
    // Check if active window changed
    xcb_window_t focus;
    int revert;
    xcb_get_input_focus_reply_t *reply = xcb_get_input_focus_reply(
        m_conn,
        xcb_get_input_focus(m_conn),
        nullptr);

    if (reply) {
        xcb_window_t newFocus = reply->focus;
        if (newFocus != m_activeWindow) {
            m_activeWindow = newFocus;

            // Restore layout for this window
            if (m_mapping.contains(m_activeWindow)) {
                switchToGroup(m_mapping[m_activeWindow]);
            }
        }
        free(reply);
    }
}

// ============================================================================
// AppKbdKeeper
// ============================================================================

AppKbdKeeper::AppKbdKeeper(xcb_connection_t *conn, int deviceId) :
    KbLayoutKeeper(conn, deviceId, KeeperType::Application)
{
}

void AppKbdKeeper::switchToGroup(uint group)
{
    // Remember current app's layout
    if (!m_activeClass.isEmpty()) {
        m_mapping[m_activeClass] = m_currentGroup;
    }

    KbLayoutKeeper::switchToGroup(group);
}

void AppKbdKeeper::layoutChanged(uint group)
{
    KbLayoutKeeper::layoutChanged(group);

    // Restore layout for current app
    if (!m_activeClass.isEmpty() && m_mapping.contains(m_activeClass)) {
        switchToGroup(m_mapping[m_activeClass]);
    }
}

void AppKbdKeeper::checkState()
{
    // Get active window and its class
    xcb_window_t focus;
    int revert;
    xcb_get_input_focus_reply_t *reply = xcb_get_input_focus_reply(
        m_conn,
        xcb_get_input_focus(m_conn),
        nullptr);

    if (reply) {
        xcb_window_t newFocus = reply->focus;
        if (newFocus != XCB_NONE && newFocus != XCB_WINDOW_NONE) {
            // Get window class (simplified - would need WM_CLASS property)
            // For now, use window ID as proxy
            QString className = QString("win_%1").arg(newFocus, 0, 16);

            if (className != m_activeClass) {
                m_activeClass = className;

                // Restore layout for this app
                if (m_mapping.contains(m_activeClass)) {
                    switchToGroup(m_mapping[m_activeClass]);
                }
            }
        }
        free(reply);
    }
}
