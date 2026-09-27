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
#include "kblayout-config.h"

// xcb/xkb.h uses 'explicit' as a C struct field name (xcb_xkb_set_explicit_t),
// which conflicts with the C++ keyword. Redefine it only during header inclusion.
#ifdef explicit
#undef explicit
#endif
#define explicit _explicit
#include <xcb/xkb.h>
#undef explicit
#include <xcb/xproto.h>
#include <xcb/xfixes.h>

// ============================================================================
// Helper: Read WM_CLASS property from X11 window
// ============================================================================

static QString readWmClassName(xcb_connection_t *conn, xcb_window_t window)
{
    // WM_CLASS is a STRING property containing two null-terminated strings:
    // instance_name\0class_name\0. We use class_name for identification.
    xcb_get_property_cookie_t cookie = xcb_get_property(
        conn,
        false,      // delete
        window,
        XCB_ATOM_WM_CLASS,
        XCB_ATOM_STRING,
        0,          // offset
        Kblayout::MaxPropertyLength);

    xcb_get_property_reply_t *reply = xcb_get_property_reply(conn, cookie, nullptr);
    if (!reply)
        return QString();

    QString result;
    if (reply->value_len > 0) {
        char *value = static_cast<char*>(xcb_get_property_value(reply));
        if (value) {
            // WM_CLASS contains "instance\0class\0", we use class (second part)
            // Find the second null terminator
            char *null1 = strchr(value, '\0');
            if (null1) {
                char *null2 = strchr(null1 + 1, '\0');
                if (null2) {
                    // Extract class name (between first and second null)
                    int len = null2 - (null1 + 1);
                    result = QString::fromUtf8(null1 + 1, len);
                } else {
                    result = QString::fromUtf8(null1 + 1);
                }
            }
        }
    }

    free(reply);
    return result;
}

KbLayoutKeeper::KbLayoutKeeper(xcb_connection_t *conn, int deviceId, KeeperType type) :
    m_conn(conn),
    m_deviceId(deviceId),
    m_type(type)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(Kblayout::TimerCheckIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &KbLayoutKeeper::checkState);
}

bool KbLayoutKeeper::setup()
{
    m_timer->start();
    return true;
}

void KbLayoutKeeper::switchToNext()
{
    // Cycle to next group using dynamic group count
    uint numGrps = numGroups();
    if (numGrps == 0)
        numGrps = Kblayout::DefaultMaxGroups;  // Fallback if not set
    switchToGroup((m_currentGroup + 1) % numGrps);
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

void KbLayoutKeeper::layoutChanged(uint group)
{
    // Override in subclasses
    Q_UNUSED(group)
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
    // Get active window
    xcb_window_t focus;
    int revert;
    xcb_get_input_focus_reply_t *reply = xcb_get_input_focus_reply(
        m_conn,
        xcb_get_input_focus(m_conn),
        nullptr);

    if (reply) {
        xcb_window_t newFocus = reply->focus;
        if (newFocus != XCB_NONE && newFocus != XCB_WINDOW_NONE) {
            // Read WM_CLASS property from the focused window
            QString className = readWmClassName(m_conn, newFocus);

            if (!className.isEmpty() && className != m_activeClass) {
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
