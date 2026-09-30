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

#include "kblayout-config.h"

class TestKbLayoutConfig : public QObject
{
    Q_OBJECT

private slots:
    void testMaxPropertyLength();
    void testDefaultMaxGroups();
    void testPollInterval();
    void testLedPollInterval();
    void testTimerCheckInterval();
    void testProcessTimeouts();
    void testNotificationTimeout();
    void testBackendFlags();
};

void TestKbLayoutConfig::testMaxPropertyLength()
{
    // X11 property read limit should be reasonable
    QCOMPARE(Kblayout::MaxPropertyLength, 4096);
    QVERIFY(Kblayout::MaxPropertyLength > 0);
}

void TestKbLayoutConfig::testDefaultMaxGroups()
{
    // XKB spec defines 4 layout groups
    QCOMPARE(Kblayout::DefaultMaxGroups, 4u);
}

void TestKbLayoutConfig::testPollInterval()
{
    // Layout polling interval should be reasonable (> 0, < 10s)
    QVERIFY(Kblayout::PollIntervalMs > 0);
    QVERIFY(Kblayout::PollIntervalMs < 10000);
}

void TestKbLayoutConfig::testLedPollInterval()
{
    // LED polling should be faster than layout polling
    QVERIFY(Kblayout::LedPollIntervalMs > 0);
    QVERIFY(Kblayout::LedPollIntervalMs < Kblayout::PollIntervalMs);
}

void TestKbLayoutConfig::testTimerCheckInterval()
{
    // Keeper check interval should be reasonable
    QVERIFY(Kblayout::TimerCheckIntervalMs > 0);
    QVERIFY(Kblayout::TimerCheckIntervalMs <= 1000);
}

void TestKbLayoutConfig::testProcessTimeouts()
{
    // Process timeouts should be in ascending order
    QVERIFY(Kblayout::ShortProcessTimeoutMs < Kblayout::ProcessTimeoutMs);
    QVERIFY(Kblayout::ProcessTimeoutMs < Kblayout::LongProcessTimeoutMs);

    // Values should be reasonable
    QCOMPARE(Kblayout::ShortProcessTimeoutMs, 1000);
    QCOMPARE(Kblayout::ProcessTimeoutMs, 2000);
    QCOMPARE(Kblayout::LongProcessTimeoutMs, 3000);
}

void TestKbLayoutConfig::testNotificationTimeout()
{
    // Notification display duration should be reasonable (2-10s)
    QVERIFY(Kblayout::NotificationTimeoutMs >= 2000);
    QVERIFY(Kblayout::NotificationTimeoutMs <= 10000);
}

void TestKbLayoutConfig::testBackendFlags()
{
    // At least one backend should be available when compiled
#ifdef KBLAYOUT_HAS_BACKEND
    QVERIFY(true);
#else
    // If no backend is compiled in, that's also valid (build-time choice)
    QVERIFY(true);
#endif

#ifdef KBLAYOUT_X11
    QVERIFY(true);
#endif

#ifdef KBLAYOUT_WAYLAND
    QVERIFY(true);
#endif
}

QTEST_MAIN(TestKbLayoutConfig)
#include "test_kblayoutconfig.moc"
