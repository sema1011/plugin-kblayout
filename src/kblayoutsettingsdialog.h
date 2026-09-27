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

class Ui_KbLayoutSettingsDialog;
class KbLayoutSettings;

/**
 * \brief Configuration dialog for the keyboard layout switcher.
 *
 * Uses Qt Designer UI file for layout.
 */
class KbLayoutSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit KbLayoutSettingsDialog(KbLayoutSettings *settings, QWidget *parent = nullptr);
    ~KbLayoutSettingsDialog() override;

private slots:
    void onApply();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();

    Ui_KbLayoutSettingsDialog *ui{nullptr};
    KbLayoutSettings *m_settings{nullptr};
};

#endif // KBLAYOUTSETTINGSDIALOG_H
