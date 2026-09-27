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

#ifndef KBLAYOUTWIDGET_H
#define KBLAYOUTWIDGET_H

#include <QWidget>

class QToolButton;
class QMenu;
class QLabel;
class ILXQtPanel;

/**
 * \brief Widget displayed on the LXQt panel.
 *
 * Shows the current keyboard layout code (e.g. "EN", "RU") and provides
 * switching via left-click (next layout) and right-click (context menu).
 * Also shows Caps/Num/Scroll LED indicators.
 *
 * Signals:
 *   - layoutNextRequested() — left click / "Next Layout" action
 *   - configureRequested()  — right click / "Configure..." action
 *   - layoutSelected(int)   — layout chosen from context menu
 */
class KbLayoutWidget : public QWidget
{
    Q_OBJECT
public:
    explicit KbLayoutWidget(QWidget *parent = nullptr);
    ~KbLayoutWidget() override;

    /**
     * \brief Set reference to the panel for popup positioning.
     */
    void setPanel(ILXQtPanel *panel);

    /**
     * \brief Setup widget based on current settings.
     */
    void setup();

    /**
     * \brief Update the list of available layouts for the context menu.
     */
    void setLayouts(const QStringList &syms, const QStringList &names);

    /**
     * \brief Update the current layout index (display + tooltip).
     */
    void setCurrentLayout(int index);

    /**
     * \brief Update LED state (Caps/Num/Scroll lock).
     */
    void setLedState(bool caps, bool num, bool scroll);

    /**
     * \brief Set LED visibility flags.
     */
    void setShowCaps(bool show);
    void setShowNum(bool show);
    void setShowScroll(bool show);

public slots:
    /**
     * \brief Update display when layout changes.
     * \param layoutIndex Index of the new current layout.
     */
    void onLayoutChanged(int layoutIndex);

signals:
    /**
     * \brief Emitted when user requests next layout (left click).
     */
    void layoutNextRequested();

    /**
     * \brief Emitted when user requests configuration dialog.
     */
    void configureRequested();

    /**
     * \brief Emitted when user selects a specific layout from context menu.
     * \param index Index of the selected layout.
     */
    void layoutSelected(int index);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void updateDisplay();
    void buildContextMenu();
    QPoint popupPosition();

    QToolButton *m_button{nullptr};
    QMenu *m_menu{nullptr};
    ILXQtPanel *m_panel{nullptr};
    QStringList m_layoutSyms;
    QStringList m_layoutNames;
    int m_currentIdx{-1};
    bool m_showText{true};
    bool m_showCaps{false};
    bool m_showNum{false};
    bool m_showScroll{false};
    int m_fontSize{9};

    // LED indicators
    QLabel *m_capsLabel{nullptr};
    QLabel *m_numLabel{nullptr};
    QLabel *m_scrollLabel{nullptr};
    bool m_capsActive{false};
    bool m_numActive{false};
    bool m_scrollActive{false};
};

#endif // KBLAYOUTWIDGET_H
