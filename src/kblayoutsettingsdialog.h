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

#ifndef KBLAYOUTSETTINGSDIALOG_H
#define KBLAYOUTSETTINGSDIALOG_H

#include <QDialog>

class KbLayoutSettings;
class QCheckBox;
class QSpinBox;

/**
 * \brief Configuration dialog for the keyboard layout switcher.
 *
 * Provides options for:
 * - Show text / flag / both
 * - Font size
 * - Show notification on switch
 * - Cycle all layouts
 */
class KbLayoutSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit KbLayoutSettingsDialog(KbLayoutSettings *settings, QWidget *parent = nullptr);
    ~KbLayoutSettingsDialog() override = default;

private slots:
    void onApply();

private:
    void setupUi();

    KbLayoutSettings *m_settings;

    QCheckBox *m_chkShowText{nullptr};
    QCheckBox *m_chkShowFlag{nullptr};
    QSpinBox *m_spinFontSize{nullptr};
    QCheckBox *m_chkNotification{nullptr};
};

#endif // KBLAYOUTSETTINGSDIALOG_H
