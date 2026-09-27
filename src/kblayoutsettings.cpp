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

#include "kblayoutsettings.h"

#include <lxqt/pluginsettings.h>

KbLayoutSettings::KbLayoutSettings() = default;
KbLayoutSettings::~KbLayoutSettings() = default;

void KbLayoutSettings::init(PluginSettings *settings)
{
    m_settings = settings;
}

bool KbLayoutSettings::showText() const
{
    if (!m_settings) return true;
    return m_settings->value(QStringLiteral("showText"), true).toBool();
}

bool KbLayoutSettings::showCaps() const
{
    if (!m_settings) return false;
    return m_settings->value(QStringLiteral("showCaps"), false).toBool();
}

bool KbLayoutSettings::showNum() const
{
    if (!m_settings) return false;
    return m_settings->value(QStringLiteral("showNum"), false).toBool();
}

bool KbLayoutSettings::showScroll() const
{
    if (!m_settings) return false;
    return m_settings->value(QStringLiteral("showScroll"), false).toBool();
}

int KbLayoutSettings::fontSize() const
{
    if (!m_settings) return 9;
    return m_settings->value(QStringLiteral("fontSize"), 9).toInt();
}

QString KbLayoutSettings::flagPattern() const
{
    if (!m_settings) return QString();
    return m_settings->value(QStringLiteral("flagPattern"), QString()).toString();
}

bool KbLayoutSettings::showNotification() const
{
    if (!m_settings) return true;
    return m_settings->value(QStringLiteral("showNotification"), true).toBool();
}

void KbLayoutSettings::setShowText(bool show)
{
    if (m_settings)
        m_settings->setValue(QStringLiteral("showText"), show);
}

void KbLayoutSettings::setShowCaps(bool show)
{
    if (m_settings)
        m_settings->setValue(QStringLiteral("showCaps"), show);
}

void KbLayoutSettings::setShowNum(bool show)
{
    if (m_settings)
        m_settings->setValue(QStringLiteral("showNum"), show);
}

void KbLayoutSettings::setShowScroll(bool show)
{
    if (m_settings)
        m_settings->setValue(QStringLiteral("showScroll"), show);
}

void KbLayoutSettings::setFontSize(int size)
{
    if (m_settings)
        m_settings->setValue(QStringLiteral("fontSize"), size);
}

void KbLayoutSettings::setFlagPattern(const QString &pattern)
{
    if (m_settings)
        m_settings->setValue(QStringLiteral("flagPattern"), pattern);
}

void KbLayoutSettings::setShowNotification(bool show)
{
    if (m_settings)
        m_settings->setValue(QStringLiteral("showNotification"), show);
}
