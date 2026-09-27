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

#ifndef KBLAYOUT_CONFIG_H
#define KBLAYOUT_CONFIG_H

// ============================================================================
// Backend availability (set via CMake -DKBLAYOUT_X11=ON/OFF/AUTO)
// ============================================================================

// KBLAYOUT_X11  — defined when X11 backend is enabled
// KBLAYOUT_WAYLAND — defined when Wayland backend is enabled
// KBLAYOUT_HAS_BACKEND — defined when at least one backend is enabled

#if defined(KBLAYOUT_X11) || defined(KBLAYOUT_WAYLAND)
#  define KBLAYOUT_HAS_BACKEND 1
#endif

// ============================================================================
// Keyboard layout plugin constants
// ============================================================================

namespace Kblayout {

// Maximum length for X11 property reads (WM_CLASS, etc.)
constexpr int MaxPropertyLength = 4096;

// Maximum number of XKB layout groups (XKB spec defines 4 groups)
constexpr uint DefaultMaxGroups = 4;

// Polling intervals (milliseconds)
constexpr int PollIntervalMs = 2000;       // Layout polling interval
constexpr int LedPollIntervalMs = 500;     // LED state polling interval
constexpr int TimerCheckIntervalMs = 500;  // Keeper check interval

// Timeout values (milliseconds)
constexpr int ProcessTimeoutMs = 2000;     // QProcess wait timeout
constexpr int ShortProcessTimeoutMs = 1000; // Short QProcess wait timeout
constexpr int LongProcessTimeoutMs = 3000; // Long QProcess wait timeout
constexpr int NotificationTimeoutMs = 5000; // Notification display duration

} // namespace Kblayout

#endif // KBLAYOUT_CONFIG_H
