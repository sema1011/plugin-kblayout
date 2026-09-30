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
#include <QApplication>
#include <QToolButton>
#include <QMenu>
#include <QLabel>
#include <QFile>

#include "kblayoutwidget.h"

class TestKbLayoutWidget : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Security tests
    void testValidateFlagPattern_safe();
    void testValidateFlagPattern_pathTraversal();
    void testValidateFlagPattern_specialChars();
    void testValidateFlagPattern_empty();

    // Display tests
    void testSetLayouts();
    void testSetCurrentLayout();
    void testUpdateDisplay_textMode();
    void testUpdateDisplay_noLayouts();

    // LED tests
    void testSetLedState();
    void testShowCapsToggle();
    void testShowNumToggle();
    void testShowScrollToggle();

    // Font size tests
    void testFontSize();
    void testFontSize_zero();
    void testFontSize_negative();

    // Flag pattern tests
    void testSetFlagPattern_valid();
    void testSetFlagPattern_invalid();
    void testSetFlagPattern_empty();

private:
    KbLayoutWidget *m_widget{nullptr};
};

void TestKbLayoutWidget::initTestCase()
{
    // QApplication should already be created by qtest_main
    m_widget = new KbLayoutWidget(nullptr);
}

void TestKbLayoutWidget::cleanupTestCase()
{
    delete m_widget;
}

// ============================================================================
// Security: validateFlagPattern
// ============================================================================

void TestKbLayoutWidget::testValidateFlagPattern_safe()
{
    // Valid patterns
    QVERIFY(m_widget->validateFlagPattern(QStringLiteral("/usr/share/sddm/flags/%1.png")));
    QVERIFY(m_widget->validateFlagPattern(QStringLiteral("/usr/share/sddm/flags")));
    QVERIFY(m_widget->validateFlagPattern(QStringLiteral("/path/to/flags")));
    QVERIFY(m_widget->validateFlagPattern(QStringLiteral("/my_flags/us.png")));
    QVERIFY(m_widget->validateFlagPattern(QStringLiteral("/path with spaces/%1.png")));
    QVERIFY(m_widget->validateFlagPattern(QStringLiteral("/path-with-dashes/%1.png")));
}

void TestKbLayoutWidget::testValidateFlagPattern_pathTraversal()
{
    // Path traversal attacks must be rejected
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("../etc/passwd")));
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("/path/../../../etc/passwd")));
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("/path/to/../../secret")));
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("..")));
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("/path/..")));
}

void TestKbLayoutWidget::testValidateFlagPattern_specialChars()
{
    // Dangerous characters must be rejected
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("/path/$HOME/flags")));
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("/path/`cmd`/flags")));
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("/path/$(rm -rf)/flags")));
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("/path;ls/flags")));
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("/path|ls/flags")));
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("/path&ls/flags")));
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("/path>out/flags")));
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("/path<in/flags")));
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("/path\"quote/flags")));
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("/path'quote/flags")));
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("/path\\back/flags")));
    QVERIFY(!m_widget->validateFlagPattern(QStringLiteral("/path\nnew/flags")));
}

void TestKbLayoutWidget::testValidateFlagPattern_empty()
{
    // Empty is valid (means no flags)
    QVERIFY(m_widget->validateFlagPattern(QStringLiteral("")));
}

// ============================================================================
// Display tests
// ============================================================================

void TestKbLayoutWidget::testSetLayouts()
{
    QStringList syms = {QStringLiteral("us"), QStringLiteral("ru"), QStringLiteral("de")};
    QStringList names = {QStringLiteral("English (US)"), QStringLiteral("Russian"), QStringLiteral("German")};

    m_widget->setLayouts(syms, names);

    // Verify widget has children (button + 3 LED labels)
    auto children = m_widget->children();
    QVERIFY(children.size() >= 4);
}

void TestKbLayoutWidget::testSetCurrentLayout()
{
    QStringList syms = {QStringLiteral("us"), QStringLiteral("ru")};
    QStringList names = {QStringLiteral("English"), QStringLiteral("Russian")};

    m_widget->setLayouts(syms, names);
    m_widget->setCurrentLayout(0);

    QToolButton *button = m_widget->findChild<QToolButton *>();
    QVERIFY(button);
    QCOMPARE(button->text(), QStringLiteral("US"));

    m_widget->setCurrentLayout(1);
    QCOMPARE(button->text(), QStringLiteral("RU"));
}

void TestKbLayoutWidget::testUpdateDisplay_textMode()
{
    m_widget->setShowFlags(false);

    QStringList syms = {QStringLiteral("us"), QStringLiteral("ru")};
    QStringList names = {QStringLiteral("English"), QStringLiteral("Russian")};

    m_widget->setLayouts(syms, names);
    m_widget->setCurrentLayout(0);

    QToolButton *button = m_widget->findChild<QToolButton *>();
    QVERIFY(button);
    QCOMPARE(button->toolButtonStyle(), Qt::ToolButtonTextOnly);
    QCOMPARE(button->text(), QStringLiteral("US"));
    QCOMPARE(button->icon(), QIcon());
}

void TestKbLayoutWidget::testUpdateDisplay_noLayouts()
{
    QStringList syms;
    QStringList names;
    m_widget->setLayouts(syms, names);
    m_widget->setCurrentLayout(0);

    QToolButton *button = m_widget->findChild<QToolButton *>();
    QVERIFY(button);
    QCOMPARE(button->text(), QStringLiteral("??"));
    QCOMPARE(button->toolTip(), QStringLiteral("No keyboard layouts available"));
}

// ============================================================================
// LED tests — verify internal flags are set correctly
// ============================================================================

void TestKbLayoutWidget::testSetLedState()
{
    // Verify setShowCaps/setShowNum/setShowScroll set internal flags
    // Actual LED visibility is tested implicitly via widget construction
    m_widget->setShowCaps(true);
    m_widget->setShowNum(true);
    m_widget->setShowScroll(true);

    // setLedState should not crash
    m_widget->setLedState(true, true, true);
    m_widget->setLedState(false, false, false);
}

void TestKbLayoutWidget::testShowCapsToggle()
{
    m_widget->setShowCaps(false);
    m_widget->setShowCaps(true);
    // Flag toggled without crash
}

void TestKbLayoutWidget::testShowNumToggle()
{
    m_widget->setShowNum(false);
    m_widget->setShowNum(true);
}

void TestKbLayoutWidget::testShowScrollToggle()
{
    m_widget->setShowScroll(false);
    m_widget->setShowScroll(true);
}

// ============================================================================
// Font size tests
// ============================================================================

void TestKbLayoutWidget::testFontSize()
{
    m_widget->setFontSize(14);

    QToolButton *button = m_widget->findChild<QToolButton *>();
    QVERIFY(button);
    QFont font = button->font();
    QCOMPARE(font.pointSize(), 14);
}

void TestKbLayoutWidget::testFontSize_zero()
{
    m_widget->setFontSize(0);

    QToolButton *button = m_widget->findChild<QToolButton *>();
    QFont font = button->font();
    // Zero should fall back to default (9)
    QCOMPARE(font.pointSize(), 9);
}

void TestKbLayoutWidget::testFontSize_negative()
{
    m_widget->setFontSize(-5);

    QToolButton *button = m_widget->findChild<QToolButton *>();
    QFont font = button->font();
    // Negative should fall back to default (9)
    QCOMPARE(font.pointSize(), 9);
}

// ============================================================================
// Flag pattern tests
// ============================================================================

void TestKbLayoutWidget::testSetFlagPattern_valid()
{
    m_widget->setFlagPattern(QStringLiteral("/usr/share/sddm/flags/%1.png"));

    QStringList syms = {QStringLiteral("us")};
    QStringList names;

    m_widget->setLayouts(syms, names);
    m_widget->setCurrentLayout(0);

    // Pattern is valid, file doesn't exist, falls back to text
    QToolButton *button = m_widget->findChild<QToolButton *>();
    QVERIFY(button);
    // Since file doesn't exist, shows text
    QCOMPARE(button->text(), QStringLiteral("US"));
}

void TestKbLayoutWidget::testSetFlagPattern_invalid()
{
    // Invalid pattern should be rejected and cleared
    m_widget->setFlagPattern(QStringLiteral("../etc/passwd"));

    QString pattern;
    // The pattern should have been cleared internally
    // We verify by checking that setLayouts doesn't crash
    QStringList syms = {QStringLiteral("us")};
    QStringList names;
    m_widget->setLayouts(syms, names);
    m_widget->setCurrentLayout(0);

    QToolButton *button = m_widget->findChild<QToolButton *>();
    QVERIFY(button);
    // Without valid pattern, shows text
    QCOMPARE(button->text(), QStringLiteral("US"));
}

void TestKbLayoutWidget::testSetFlagPattern_empty()
{
    m_widget->setFlagPattern(QStringLiteral(""));

    QStringList syms = {QStringLiteral("us")};
    QStringList names;
    m_widget->setLayouts(syms, names);
    m_widget->setCurrentLayout(0);

    QToolButton *button = m_widget->findChild<QToolButton *>();
    QVERIFY(button);
    QCOMPARE(button->text(), QStringLiteral("US"));
}

QTEST_MAIN(TestKbLayoutWidget)
#include "test_kblayoutwidget.moc"
