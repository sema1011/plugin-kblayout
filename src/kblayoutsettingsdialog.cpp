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

#include <QCheckBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>

KbLayoutSettingsDialog::KbLayoutSettingsDialog(KbLayoutSettings *settings, QWidget *parent) :
    QDialog(parent),
    m_settings(settings)
{
    setWindowTitle(tr("Keyboard Layout Switcher — Settings"));
    setAttribute(Qt::WA_DeleteOnClose);

    setupUi();
}

void KbLayoutSettingsDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // Display group
    auto *displayGroup = new QGroupBox(tr("Display"), this);
    auto *displayLayout = new QVBoxLayout(displayGroup);

    m_chkShowText = new QCheckBox(tr("Show layout text (e.g. EN, RU)"), displayGroup);
    m_chkShowText->setChecked(m_settings->showText());
    displayLayout->addWidget(m_chkShowText);

    m_chkShowFlag = new QCheckBox(tr("Show flag icon"), displayGroup);
    m_chkShowFlag->setChecked(m_settings->showFlag());
    m_chkShowFlag->setEnabled(false); // TODO: implement flag support
    displayLayout->addWidget(m_chkShowFlag);

    auto *fontSizeLayout = new QHBoxLayout();
    fontSizeLayout->addWidget(new QLabel(tr("Font size:"), displayGroup));
    m_spinFontSize = new QSpinBox(displayGroup);
    m_spinFontSize->setRange(6, 24);
    m_spinFontSize->setValue(m_settings->fontSize());
    fontSizeLayout->addWidget(m_spinFontSize);
    displayLayout->addLayout(fontSizeLayout);

    mainLayout->addWidget(displayGroup);

    // Behavior group
    auto *behaviorGroup = new QGroupBox(tr("Behavior"), this);
    auto *behaviorLayout = new QVBoxLayout(behaviorGroup);

    m_chkNotification = new QCheckBox(tr("Show notification on layout switch"), behaviorGroup);
    m_chkNotification->setChecked(m_settings->showNotification());
    behaviorLayout->addWidget(m_chkNotification);

    mainLayout->addWidget(behaviorGroup);

    // Buttons
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Apply | QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::clicked, this, [this, buttons](QAbstractButton *btn) {
        QPushButton *applyBtn = buttons->button(QDialogButtonBox::Apply);
        if (applyBtn && applyBtn == qobject_cast<QPushButton*>(btn)) {
            onApply();
        }
    });

    mainLayout->addWidget(buttons);
}

void KbLayoutSettingsDialog::onApply()
{
    m_settings->setShowText(m_chkShowText->isChecked());
    m_settings->setShowFlag(m_chkShowFlag->isChecked());
    m_settings->setFontSize(m_spinFontSize->value());
    m_settings->setShowNotification(m_chkNotification->isChecked());
}
