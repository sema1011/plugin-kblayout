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

#include "kblayoutwidget.h"
#include "kblayoutbackend.h"

#include <QToolButton>
#include <QMenu>
#include <QMouseEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QAction>
#include <QScreen>
#include <QGuiApplication>
#include <lxqt/ilxqtpanel.h>

KbLayoutWidget::KbLayoutWidget(QWidget *parent) :
    QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(2, 0, 2, 0);
    mainLayout->setSpacing(2);

    // Layout button
    m_button = new QToolButton(this);
    m_button->setPopupMode(QToolButton::InstantPopup);
    m_button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_button->setFont(QFont(QStringLiteral("Sans"), m_fontSize, QFont::Bold));
    m_menu = new QMenu(this);
    m_button->setMenu(m_menu);
    mainLayout->addWidget(m_button);

    // LED indicators (initially hidden)
    m_capsLabel = new QLabel("Caps", this);
    m_capsLabel->setFixedSize(24, 16);
    m_capsLabel->setStyleSheet("color: gray; font-size: 9px;");
    m_capsLabel->setVisible(false);
    mainLayout->addWidget(m_capsLabel);

    m_numLabel = new QLabel("Num", this);
    m_numLabel->setFixedSize(24, 16);
    m_numLabel->setStyleSheet("color: gray; font-size: 9px;");
    m_numLabel->setVisible(false);
    mainLayout->addWidget(m_numLabel);

    m_scrollLabel = new QLabel("Scr", this);
    m_scrollLabel->setFixedSize(24, 16);
    m_scrollLabel->setStyleSheet("color: gray; font-size: 9px;");
    m_scrollLabel->setVisible(false);
    mainLayout->addWidget(m_scrollLabel);

    updateDisplay();
}

KbLayoutWidget::~KbLayoutWidget() = default;

void KbLayoutWidget::setPanel(ILXQtPanel *panel)
{
    m_panel = panel;
}

void KbLayoutWidget::setup()
{
    m_button->setFont(QFont(QStringLiteral("Sans"), m_fontSize, QFont::Bold));
    updateDisplay();
}

void KbLayoutWidget::setLayouts(const QStringList &syms, const QStringList &names)
{
    m_layoutSyms = syms;
    m_layoutNames = names;
    updateDisplay();
}

void KbLayoutWidget::setCurrentLayout(int index)
{
    m_currentIdx = index;
    updateDisplay();
}

void KbLayoutWidget::setLedState(bool caps, bool num, bool scroll)
{
    bool changed = (caps != m_capsActive || num != m_numActive || scroll != m_scrollActive);

    m_capsActive = caps;
    m_numActive = num;
    m_scrollActive = scroll;

    // Update Caps LED
    if (m_showCaps) {
        m_capsLabel->setVisible(true);
        m_capsLabel->setStyleSheet(caps
            ? "color: green; font-size: 9px;"
            : "color: gray; font-size: 9px;");
    }

    // Update Num LED
    if (m_showNum) {
        m_numLabel->setVisible(true);
        m_numLabel->setStyleSheet(num
            ? "color: green; font-size: 9px;"
            : "color: gray; font-size: 9px;");
    }

    // Update Scroll LED
    if (m_showScroll) {
        m_scrollLabel->setVisible(true);
        m_scrollLabel->setStyleSheet(scroll
            ? "color: green; font-size: 9px;"
            : "color: gray; font-size: 9px;");
    }
}

void KbLayoutWidget::setShowCaps(bool show)
{
    m_showCaps = show;
    if (!show && m_capsLabel)
        m_capsLabel->setVisible(false);
}

void KbLayoutWidget::setShowNum(bool show)
{
    m_showNum = show;
    if (!show && m_numLabel)
        m_numLabel->setVisible(false);
}

void KbLayoutWidget::setShowScroll(bool show)
{
    m_showScroll = show;
    if (!show && m_scrollLabel)
        m_scrollLabel->setVisible(false);
}

    // Update Num LED
    if (m_showNum) {
        m_numLabel->setVisible(true);
        m_numLabel->setStyleSheet(num
            ? "color: green; font-size: 9px;"
            : "color: gray; font-size: 9px;");
    }

    // Update Scroll LED
    if (m_showScroll) {
        m_scrollLabel->setVisible(true);
        m_scrollLabel->setStyleSheet(scroll
            ? "color: green; font-size: 9px;"
            : "color: gray; font-size: 9px;");
    }

    if (changed) {
        // Emit signal if needed
    }
}

void KbLayoutWidget::onLayoutChanged(int layoutIndex)
{
    m_currentIdx = layoutIndex;
    updateDisplay();
}

void KbLayoutWidget::updateDisplay()
{
    if (!m_layoutSyms.isEmpty() && m_currentIdx >= 0 && m_currentIdx < m_layoutSyms.size()) {
        QString display = m_layoutNames.isEmpty()
                ? m_layoutSyms[m_currentIdx].toUpper()
                : m_layoutNames[m_currentIdx];

        if (m_showText) {
            m_button->setText(display);
        } else {
            m_button->setText(m_layoutSyms[m_currentIdx].toUpper());
        }

        // Tooltip: full layout info + controls
        QString tooltip;
        if (!m_layoutNames.isEmpty() && m_currentIdx < m_layoutNames.size()) {
            tooltip = QString("%1 (%2)")
                    .arg(m_layoutNames[m_currentIdx])
                    .arg(m_layoutSyms[m_currentIdx].toUpper());
        } else {
            tooltip = m_layoutSyms[m_currentIdx].toUpper();
        }
        tooltip += QStringLiteral("\n---\n") + tr("Left click: next layout")
                   + QStringLiteral("\n") + tr("Right click: select layout");
        m_button->setToolTip(tooltip);
    } else {
        m_button->setText(tr("??"));
        m_button->setToolTip(tr("No keyboard layouts available"));
    }

    buildContextMenu();
}

void KbLayoutWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit layoutNextRequested();
    }
    QWidget::mousePressEvent(event);
}

void KbLayoutWidget::contextMenuEvent(QContextMenuEvent *event)
{
    buildContextMenu();

    // Use panel's popup positioning if available (Wayland compatibility)
    QPoint pos = popupPosition();
    m_menu->exec(pos);
}

QPoint KbLayoutWidget::popupPosition()
{
    // Try panel's calculatePopupWindowPos first (proper Wayland support)
    if (m_panel) {
        QRect rect = m_panel->calculatePopupWindowPos(
            geometry().bottomRight(), m_menu->sizeHint());
        return rect.topLeft();
    }

    // Fallback: show below the widget
    return mapToGlobal(QPoint(0, height()));
}

void KbLayoutWidget::buildContextMenu()
{
    m_menu->clear();

    // "Next Layout" action
    QAction *nextAction = m_menu->addAction(tr("Next Layout"));
    nextAction->setShortcut(QKeyCombination(Qt::CTRL, Qt::Key_Space));
    connect(nextAction, &QAction::triggered, this, [this]() {
        emit layoutNextRequested();
    });

    // Separator
    m_menu->addSeparator();

    // Individual layout actions
    for (int i = 0; i < m_layoutSyms.size(); ++i) {
        QString label = m_layoutNames.isEmpty()
                ? m_layoutSyms[i].toUpper()
                : QString("%1 (%2)").arg(m_layoutNames[i]).arg(m_layoutSyms[i].toUpper());

        QAction *action = m_menu->addAction(label);

        // Mark current layout
        if (i == m_currentIdx) {
            action->setCheckable(true);
            action->setChecked(true);
        }

        int idx = i;
        connect(action, &QAction::triggered, this, [this, idx]() {
            emit layoutSelected(idx);
        });
    }

    if (!m_layoutSyms.isEmpty()) {
        m_menu->addSeparator();
    }

    // "Configure..." action
    QAction *configAction = m_menu->addAction(tr("Configure..."));
    connect(configAction, &QAction::triggered, this, [this]() {
        emit configureRequested();
    });
}
