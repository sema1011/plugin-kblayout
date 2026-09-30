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

#include "kblayoutsettingsdialog.h"
#include "kblayoutsettings.h"

#include "ui_kblayoutsettingsdialog.h"
#include <QPushButton>
#include <QProcess>

KbLayoutSettingsDialog::KbLayoutSettingsDialog(KbLayoutSettings *settings, QWidget *parent) :
    QDialog(parent),
    m_settings(settings),
    ui(new Ui_KbLayoutSettingsDialog)
{
    setWindowTitle(tr("Keyboard Layout Switcher — Settings"));
    setAttribute(Qt::WA_DeleteOnClose);

    ui->setupUi(this);
    loadSettings();

    // Connect Apply and Close buttons
    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton *btn) {
        QPushButton *applyBtn = ui->buttonBox->button(QDialogButtonBox::Apply);
        if (applyBtn && btn == applyBtn) {
            onApply();
        } else {
            saveSettings();
        }
    });

    // Connect Configure layouts button
    connect(ui->btnConfigureLayouts, &QPushButton::clicked, this, []() {
        QProcess::startDetached(QStringLiteral("lxqt-config-input"));
    });
}

KbLayoutSettingsDialog::~KbLayoutSettingsDialog() = default;

void KbLayoutSettingsDialog::loadSettings()
{
    if (!m_settings)
        return;

    ui->chkShowCaps->setChecked(m_settings->showCaps());
    ui->chkShowNum->setChecked(m_settings->showNum());
    ui->chkShowScroll->setChecked(m_settings->showScroll());
    ui->chkShowFlags->setChecked(m_settings->showFlags());
    ui->txtFlagPattern->setText(m_settings->flagPattern());
    ui->spinFontSize->setValue(m_settings->fontSize());
    ui->chkNotification->setChecked(m_settings->showNotification());
}

void KbLayoutSettingsDialog::saveSettings()
{
    if (!m_settings)
        return;

    m_settings->setShowCaps(ui->chkShowCaps->isChecked());
    m_settings->setShowNum(ui->chkShowNum->isChecked());
    m_settings->setShowScroll(ui->chkShowScroll->isChecked());
    m_settings->setShowFlags(ui->chkShowFlags->isChecked());
    m_settings->setFlagPattern(ui->txtFlagPattern->text());
    m_settings->setFontSize(ui->spinFontSize->value());
    m_settings->setShowNotification(ui->chkNotification->isChecked());
}

void KbLayoutSettingsDialog::onApply()
{
    saveSettings();
}
