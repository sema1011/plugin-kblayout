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
#include <QLabel>
#include <QAction>
#include <QFile>
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
    m_button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_button->setFont(QFont(QStringLiteral("Sans"), m_fontSize, QFont::Bold));
    m_button->setContextMenuPolicy(Qt::NoContextMenu);
    m_button->setIconSize(QSize(24, 24));  // Explicit icon size for flag display
    mainLayout->addWidget(m_button);

    // Install event filter for both left and right click handling
    m_button->installEventFilter(this);

    // LED indicators (initially hidden)

    // Install event filter for right-click menu
    m_button->installEventFilter(this);

    // LED indicators (initially hidden)
    m_capsLabel = new QLabel(QStringLiteral("Caps"), this);
    m_capsLabel->setFixedSize(32, 18);
    m_capsLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 12px; font-weight: bold;"));
    m_capsLabel->setVisible(false);
    mainLayout->addWidget(m_capsLabel);

    m_numLabel = new QLabel(QStringLiteral("Num"), this);
    m_numLabel->setFixedSize(32, 18);
    m_numLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 12px; font-weight: bold;"));
    m_numLabel->setVisible(false);
    mainLayout->addWidget(m_numLabel);

    m_scrollLabel = new QLabel(QStringLiteral("Scr"), this);
    m_scrollLabel->setFixedSize(32, 18);
    m_scrollLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 12px; font-weight: bold;"));
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
    m_capsActive = caps;
    m_numActive = num;
    m_scrollActive = scroll;

    // Update Caps LED
    if (m_showCaps) {
        m_capsLabel->setVisible(true);
        m_capsLabel->setStyleSheet(caps
            ? QStringLiteral("color: green; font-size: 12px; font-weight: bold;")
            : QStringLiteral("color: gray; font-size: 12px; font-weight: bold;"));
    }

    // Update Num LED
    if (m_showNum) {
        m_numLabel->setVisible(true);
        m_numLabel->setStyleSheet(num
            ? QStringLiteral("color: green; font-size: 12px; font-weight: bold;")
            : QStringLiteral("color: gray; font-size: 12px; font-weight: bold;"));
    }

    // Update Scroll LED
    if (m_showScroll) {
        m_scrollLabel->setVisible(true);
        m_scrollLabel->setStyleSheet(scroll
            ? QStringLiteral("color: green; font-size: 12px; font-weight: bold;")
            : QStringLiteral("color: gray; font-size: 12px; font-weight: bold;"));
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

void KbLayoutWidget::setFontSize(int size)
{
    if (size <= 0)
        size = 9;
    m_fontSize = size;
    m_button->setFont(QFont(QStringLiteral("Sans"), m_fontSize, QFont::Bold));
    updateDisplay();
}

void KbLayoutWidget::setShowText(bool show)
{
    m_showText = show;
    updateDisplay();
}

void KbLayoutWidget::setFlagPattern(const QString &pattern)
{
    m_flagPattern = pattern;
    updateDisplay();
}

void KbLayoutWidget::setShowFlags(bool show)
{
    m_showFlags = show;
    updateDisplay();
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

        // Try to load flag icon (same as kbindicator)
        QIcon layoutIcon;
        if (m_showFlags && !m_flagPattern.isEmpty()) {
            QString flagFile;
            if (m_flagPattern.contains(QStringLiteral("%1"))) {
                // Pattern with placeholder: /path/to/flags/%1.png
                flagFile = m_flagPattern.arg(m_layoutSyms[m_currentIdx].toLower());
            } else {
                // Directory path: /path/to/flags -> /path/to/flags/us.png
                flagFile = m_flagPattern + "/" + m_layoutSyms[m_currentIdx].toLower() + ".png";
            }
            qDebug() << "kblayout: flag file:" << flagFile << "exists:" << QFile::exists(flagFile);
            if (QFile::exists(flagFile)) {
                layoutIcon = QIcon(flagFile);
                qDebug() << "kblayout: icon availableSizes:" << layoutIcon.availableSizes();
                // Verify icon produces a pixmap (use fixed size since iconSize() is 0,0 by default)
                QPixmap pm = layoutIcon.pixmap(24, 24);
                qDebug() << "kblayout: pixmap 24x24 isNull:" << pm.isNull() << "size:" << pm.size();
                if (pm.isNull()) {
                    layoutIcon = QIcon();
                }
            }
        }

        if (!layoutIcon.pixmap(24, 24).isNull()) {
            // Icon mode: show flag icon only (like kbindicator)
            m_button->setIcon(layoutIcon);
            m_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        } else if (m_showText) {
            // Text mode: show layout symbol
            m_button->setIcon(QIcon());
            m_button->setToolButtonStyle(Qt::ToolButtonTextOnly);
            m_button->setText(m_layoutSyms[m_currentIdx].toUpper());
        } else {
            // Neither icon nor text: hide button text
            m_button->setIcon(QIcon());
            m_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
            m_button->setText(QString());
        }

        // Tooltip: full layout info + controls
        QString tooltip;
        if (!m_layoutNames.isEmpty() && m_currentIdx < m_layoutNames.size()) {
            tooltip = QStringLiteral("%1 (%2)")
                    .arg(m_layoutNames[m_currentIdx])
                    .arg(m_layoutSyms[m_currentIdx].toUpper());
        } else {
            tooltip = m_layoutSyms[m_currentIdx].toUpper();
        }
        tooltip += QStringLiteral("\n---\n") + tr("Left click: next layout")
                   + QStringLiteral("\n") + tr("Right click: menu");
        m_button->setToolTip(tooltip);
    } else {
        m_button->setText(tr("??"));
        m_button->setToolTip(tr("No keyboard layouts available"));
    }
}

bool KbLayoutWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_button && event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            // Left click: switch to next layout
            emit layoutNextRequested();
            return true;
        } else if (me->button() == Qt::RightButton) {
            // Right click: show context menu
            buildContextMenu();
            QPoint pos = popupPosition();
            m_menu->exec(pos);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void KbLayoutWidget::buildContextMenu()
{
    m_menu->clear();

    // "Next Layout" action
    QAction *nextAction = m_menu->addAction(tr("Next Layout"));
    connect(nextAction, &QAction::triggered, this, [this]() {
        emit layoutNextRequested();
    });

    // Separator
    m_menu->addSeparator();

    // Individual layout actions
    for (int i = 0; i < m_layoutSyms.size(); ++i) {
        QString label = m_layoutNames.isEmpty()
                ? m_layoutSyms[i].toUpper()
                : QStringLiteral("%1 (%2)").arg(m_layoutNames[i]).arg(m_layoutSyms[i].toUpper());

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

QPoint KbLayoutWidget::popupPosition()
{
    // Show menu below the button at its global position
    return m_button->mapToGlobal(QPoint(0, m_button->height()));
}
