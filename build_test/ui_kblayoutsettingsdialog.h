/********************************************************************************
** Form generated from reading UI file 'kblayoutsettingsdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.11.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_KBLAYOUTSETTINGSDIALOG_H
#define UI_KBLAYOUTSETTINGSDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_KbLayoutSettingsDialog
{
public:
    QVBoxLayout *verticalLayout;
    QGroupBox *displayGroup;
    QVBoxLayout *displayLayout;
    QCheckBox *chkShowText;
    QCheckBox *chkShowCaps;
    QCheckBox *chkShowNum;
    QCheckBox *chkShowScroll;
    QCheckBox *chkShowFlags;
    QHBoxLayout *flagPatternLayout;
    QLabel *labelFlagPattern;
    QLineEdit *txtFlagPattern;
    QHBoxLayout *fontSizeLayout;
    QLabel *labelFontSize;
    QSpinBox *spinFontSize;
    QGroupBox *behaviorGroup;
    QVBoxLayout *behaviorLayout;
    QCheckBox *chkNotification;
    QPushButton *btnConfigureLayouts;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *KbLayoutSettingsDialog)
    {
        if (KbLayoutSettingsDialog->objectName().isEmpty())
            KbLayoutSettingsDialog->setObjectName("KbLayoutSettingsDialog");
        KbLayoutSettingsDialog->resize(320, 240);
        verticalLayout = new QVBoxLayout(KbLayoutSettingsDialog);
        verticalLayout->setObjectName("verticalLayout");
        displayGroup = new QGroupBox(KbLayoutSettingsDialog);
        displayGroup->setObjectName("displayGroup");
        displayLayout = new QVBoxLayout(displayGroup);
        displayLayout->setObjectName("displayLayout");
        chkShowText = new QCheckBox(displayGroup);
        chkShowText->setObjectName("chkShowText");
        chkShowText->setChecked(true);

        displayLayout->addWidget(chkShowText);

        chkShowCaps = new QCheckBox(displayGroup);
        chkShowCaps->setObjectName("chkShowCaps");

        displayLayout->addWidget(chkShowCaps);

        chkShowNum = new QCheckBox(displayGroup);
        chkShowNum->setObjectName("chkShowNum");

        displayLayout->addWidget(chkShowNum);

        chkShowScroll = new QCheckBox(displayGroup);
        chkShowScroll->setObjectName("chkShowScroll");

        displayLayout->addWidget(chkShowScroll);

        chkShowFlags = new QCheckBox(displayGroup);
        chkShowFlags->setObjectName("chkShowFlags");
        chkShowFlags->setChecked(true);

        displayLayout->addWidget(chkShowFlags);

        flagPatternLayout = new QHBoxLayout();
        flagPatternLayout->setObjectName("flagPatternLayout");
        labelFlagPattern = new QLabel(displayGroup);
        labelFlagPattern->setObjectName("labelFlagPattern");

        flagPatternLayout->addWidget(labelFlagPattern);

        txtFlagPattern = new QLineEdit(displayGroup);
        txtFlagPattern->setObjectName("txtFlagPattern");

        flagPatternLayout->addWidget(txtFlagPattern);


        displayLayout->addLayout(flagPatternLayout);

        fontSizeLayout = new QHBoxLayout();
        fontSizeLayout->setObjectName("fontSizeLayout");
        labelFontSize = new QLabel(displayGroup);
        labelFontSize->setObjectName("labelFontSize");

        fontSizeLayout->addWidget(labelFontSize);

        spinFontSize = new QSpinBox(displayGroup);
        spinFontSize->setObjectName("spinFontSize");
        spinFontSize->setMinimum(6);
        spinFontSize->setMaximum(24);
        spinFontSize->setValue(9);

        fontSizeLayout->addWidget(spinFontSize);


        displayLayout->addLayout(fontSizeLayout);


        verticalLayout->addWidget(displayGroup);

        behaviorGroup = new QGroupBox(KbLayoutSettingsDialog);
        behaviorGroup->setObjectName("behaviorGroup");
        behaviorLayout = new QVBoxLayout(behaviorGroup);
        behaviorLayout->setObjectName("behaviorLayout");
        chkNotification = new QCheckBox(behaviorGroup);
        chkNotification->setObjectName("chkNotification");
        chkNotification->setChecked(true);

        behaviorLayout->addWidget(chkNotification);

        btnConfigureLayouts = new QPushButton(behaviorGroup);
        btnConfigureLayouts->setObjectName("btnConfigureLayouts");

        behaviorLayout->addWidget(btnConfigureLayouts);


        verticalLayout->addWidget(behaviorGroup);

        buttonBox = new QDialogButtonBox(KbLayoutSettingsDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Apply|QDialogButtonBox::Close);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(KbLayoutSettingsDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, KbLayoutSettingsDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, KbLayoutSettingsDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(KbLayoutSettingsDialog);
    } // setupUi

    void retranslateUi(QDialog *KbLayoutSettingsDialog)
    {
        KbLayoutSettingsDialog->setWindowTitle(QCoreApplication::translate("KbLayoutSettingsDialog", "Keyboard Layout Switcher \342\200\224 Settings", nullptr));
        displayGroup->setTitle(QCoreApplication::translate("KbLayoutSettingsDialog", "Display", nullptr));
        chkShowText->setText(QCoreApplication::translate("KbLayoutSettingsDialog", "Show layout text (e.g. EN, RU)", nullptr));
        chkShowCaps->setText(QCoreApplication::translate("KbLayoutSettingsDialog", "Show Caps Lock indicator", nullptr));
        chkShowNum->setText(QCoreApplication::translate("KbLayoutSettingsDialog", "Show Num Lock indicator", nullptr));
        chkShowScroll->setText(QCoreApplication::translate("KbLayoutSettingsDialog", "Show Scroll Lock indicator", nullptr));
        chkShowFlags->setText(QCoreApplication::translate("KbLayoutSettingsDialog", "Show flag icons", nullptr));
        labelFlagPattern->setText(QCoreApplication::translate("KbLayoutSettingsDialog", "Flag icon pattern:", nullptr));
        txtFlagPattern->setPlaceholderText(QCoreApplication::translate("KbLayoutSettingsDialog", "e.g. /usr/share/pixmaps/flags/%1.png", nullptr));
        labelFontSize->setText(QCoreApplication::translate("KbLayoutSettingsDialog", "Font size:", nullptr));
        behaviorGroup->setTitle(QCoreApplication::translate("KbLayoutSettingsDialog", "Behavior", nullptr));
        chkNotification->setText(QCoreApplication::translate("KbLayoutSettingsDialog", "Show notification on layout switch", nullptr));
        btnConfigureLayouts->setText(QCoreApplication::translate("KbLayoutSettingsDialog", "Configure layouts...", nullptr));
    } // retranslateUi

};

namespace Ui {
    class KbLayoutSettingsDialog: public Ui_KbLayoutSettingsDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_KBLAYOUTSETTINGSDIALOG_H
