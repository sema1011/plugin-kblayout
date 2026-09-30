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

#include <QTest>

#include "kblayoutsettings.h"

class TestKbLayoutSettings : public QObject
{
    Q_OBJECT

private slots:
    // Default values (no settings initialized)
    void testDefaults_noSettings();
};

// ============================================================================
// Default values (no settings initialized)
// ============================================================================

void TestKbLayoutSettings::testDefaults_noSettings()
{
    // When m_settings is nullptr, defaults should be returned
    KbLayoutSettings uninitSettings;

    QCOMPARE(uninitSettings.showCaps(), false);
    QCOMPARE(uninitSettings.showNum(), false);
    QCOMPARE(uninitSettings.showScroll(), false);
    QCOMPARE(uninitSettings.fontSize(), 9);
    QCOMPARE(uninitSettings.showFlags(), true);
    QCOMPARE(uninitSettings.showNotification(), true);
    QVERIFY(uninitSettings.flagPattern().isEmpty());
}

QTEST_MAIN(TestKbLayoutSettings)
#include "test_kblayoutsettings.moc"
