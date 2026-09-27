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

#include "waylandbackend.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QSettings>

WaylandBackend::WaylandBackend(QObject *parent) :
    KbLayoutBackend(parent)
{
    init();
}

WaylandBackend::~WaylandBackend()
{
    if (m_kwinLayouts)
        delete m_kwinLayouts;
    if (m_pollTimer)
        delete m_pollTimer;
}

bool WaylandBackend::init()
{
    QString sessionType = qEnvironmentVariable("XDG_SESSION_TYPE");
    if (sessionType != QLatin1String("wayland")) {
        qWarning() << "kblayout: Not a Wayland session (XDG_SESSION_TYPE="
                   << sessionType << ")";
        return false;
    }

    // 1. KWin — D-Bus org.kde.KeyboardLayouts
    if (QDBusConnection::sessionBus().isConnected()) {
        QDBusInterface test(QStringLiteral("org.kde.KWin"),
                            QStringLiteral("/Layouts"),
                            QStringLiteral("org.freedesktop.DBus.Introspectable"));
        if (test.isValid()) {
            m_compositor = QStringLiteral("kwin");
            return initKWin();
        }
    }

    // 2. Sway — SWAYSOCK
    QString swaySocket = qEnvironmentVariable("SWAYSOCK");
    if (!swaySocket.isEmpty() && QFile::exists(swaySocket)) {
        m_compositor = QStringLiteral("sway");
        return initSway();
    }

    // 3. Hyprland — HYPRLAND_INSTANCE_SIGNATURE
    QString hyprSig = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
    if (!hyprSig.isEmpty()) {
        m_compositor = QStringLiteral("hyprland");
        return initHyprland();
    }

    // 4. Labwc — check if labwc is running
    // Labwc doesn't set a specific env var, but we can check if wtype is available
    // and fall through to generic fallback
    m_compositor = QStringLiteral("generic");
    qWarning() << "kblayout: Wayland compositor not detected (session type: wayland)";
    return initGeneric();
}

// ============================================================================
// KWin
// ============================================================================

bool WaylandBackend::initKWin()
{
    m_kwinLayouts = new QDBusInterface(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Layouts"),
        QStringLiteral("org.kde.KeyboardLayouts"),
        QDBusConnection::sessionBus(),
        this);

    if (!m_kwinLayouts->isValid()) {
        qWarning() << "kblayout: KWin D-Bus interface not available";
        delete m_kwinLayouts;
        m_kwinLayouts = nullptr;
        return false;
    }

    // Connect to layoutChanged signal
    QDBusConnection::sessionBus().connect(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Layouts"),
        QStringLiteral("org.kde.KeyboardLayouts"),
        QStringLiteral("layoutChanged"),
        this,
        SLOT(_on_kwin_layoutChanged(uint)));

    readKWinLayouts();
    m_valid = !m_layoutSyms.isEmpty();
    if (!m_valid) {
        // Fallback: read from kxkbrc config
        readKXkbConfig();
        m_valid = !m_layoutSyms.isEmpty();
    }
    if (m_valid) {
        qInfo() << "kblayout: KWin backend initialized,"
                << m_layoutSyms.size() << "layouts found";
    } else {
        qWarning() << "kblayout: KWin backend initialized but no layouts found";
    }
    return m_valid;
}

void WaylandBackend::readKWinLayouts()
{
    if (!m_kwinLayouts)
        return;

    QDBusReply<QList<QList<QVariant>>> reply =
        m_kwinLayouts->call(QStringLiteral("getLayoutsList"));
    if (reply.isValid()) {
        m_layoutSyms.clear();
        m_layoutNames.clear();
        for (const auto &layout : reply.value()) {
            if (layout.size() >= 2) {
                m_layoutNames.append(layout[0].toString());  // display name
                m_layoutSyms.append(layout[1].toString());    // sym (e.g. "us", "ru")
            }
        }
    }
}

void WaylandBackend::readKXkbConfig()
{
    // Read from kxkbrc config (KDE keyboard settings)
    QString configPath = QStandardPaths::locate(
        QStandardPaths::ConfigLocation,
        QStringLiteral("kxkbrc"));
    if (configPath.isEmpty())
        return;

    QSettings settings(configPath, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Layout"));

    QString layout = settings.value(QStringLiteral("LayoutList")).toString();
    if (layout.isEmpty())
        return;

    // Parse comma-separated layout list (e.g. "us,ru,de")
    const auto parts = layout.split(',', Qt::SkipEmptyParts);
    m_layoutSyms.clear();
    m_layoutNames.clear();

    for (const auto &sym : parts) {
        m_layoutSyms.append(sym.trimmed().toLower());
        // Try to get display name from variant
        QString variant = settings.value(
            QStringLiteral("VariantList") + '_' + sym.trimmed()).toString();
        m_layoutNames.append(variant.isEmpty() ? sym.trimmed().toUpper()
                                                : variant.trimmed());
    }

    // Read current layout index
    QString current = settings.value(QStringLiteral("Use")).toString();
    if (!current.isEmpty()) {
        int idx = parts.indexOf(current.trimmed());
        if (idx >= 0)
            m_currentIdx = idx;
    }

    settings.endGroup();
}

// ============================================================================
// Sway
// ============================================================================

bool WaylandBackend::initSway()
{
    qInfo() << "kblayout: Sway backend initialized";

    // Start polling timer for layout changes
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(m_pollInterval);
    connect(m_pollTimer, &QTimer::timeout,
            this, &WaylandBackend::_on_poll_timer);
    m_pollTimer->start();

    readSwayLayouts();
    return true;
}

void WaylandBackend::readSwayLayouts()
{
    m_layoutSyms.clear();
    m_layoutNames.clear();

    // Try swaymsg first (most reliable when keyboard has focus)
    QProcess proc;
    proc.start(QStringLiteral("swaymsg"),
               {QStringLiteral("-t"), QStringLiteral("get_inputs")});
    proc.waitForFinished(3000);

    if (proc.exitCode() == 0) {
        QByteArray output = proc.readAllStandardOutput();
        parseSwayJson(QString::fromUtf8(output));
        if (!m_layoutSyms.isEmpty())
            return;
    }

    // Fallback: read from sway config file
    readSwayConfigLayouts();
}

void WaylandBackend::readSwayConfigLayouts()
{
    // Parse ~/.config/sway/config for "input *" xkb_layout directives
    QStringList configPaths = {
        QStringLiteral("%1/.config/sway/config").arg(
            QProcessEnvironment::systemEnvironment().value(QStringLiteral("HOME"))),
        QStringLiteral("/etc/sway/config")
    };

    for (const auto &configPath : configPaths) {
        QFile file(configPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            // Match: input * xkb_layout us,ru,de
            QRegularExpression re(QStringLiteral(R"(input\s+\*\s+xkb_layout\s+([\w,]+))"));
            auto match = re.match(line);
            if (match.hasMatch()) {
                QStringList layouts = match.captured(1).split(QLatin1Char(','));
                for (const auto &l : layouts) {
                    if (!m_layoutSyms.contains(l))
                        m_layoutSyms.append(l);
                }
                qInfo() << "kblayout: Read Sway layouts from config:" << m_layoutSyms;
                return;
            }
        }
        file.close();
    }

    qWarning() << "kblayout: Could not read Sway layout config";
}

void WaylandBackend::parseSwayJson(const QString &json)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "kblayout: Failed to parse sway JSON:" << parseError.errorString();
        return;
    }

    auto array = doc.array();
    for (const auto &value : array) {
        auto obj = value.toObject();
        QString type = obj.value(QStringLiteral("type")).toString();
        if (type != QStringLiteral("Input"))
            continue;

        QString inputType = obj.value(QStringLiteral("input_type")).toString();
        if (inputType != QStringLiteral("keyboard"))
            continue;

        // Read full layout list from xkb_layouts (not just active)
        auto layouts = obj.value(QStringLiteral("xkb_layouts")).toArray();
        if (!layouts.isEmpty()) {
            for (const auto &l : layouts) {
                m_layoutSyms.append(l.toString());
            }
        }

        // Read current active layout for polling comparison
        m_activeLayoutName = obj.value(QStringLiteral("xkb_active_layout")).toString();

        // If we already have layouts from this keyboard, we're done
        if (!m_layoutSyms.isEmpty())
            return;
    }
}

// ============================================================================
// Hyprland
// ============================================================================

bool WaylandBackend::initHyprland()
{
    qInfo() << "kblayout: Hyprland backend initialized";

    // Start polling timer for layout changes
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(m_pollInterval);
    connect(m_pollTimer, &QTimer::timeout,
            this, &WaylandBackend::_on_poll_timer);
    m_pollTimer->start();

    readHyprlandLayouts();
    return true;
}

void WaylandBackend::readHyprlandLayouts()
{
    m_layoutSyms.clear();
    m_layoutNames.clear();

    QProcess proc;
    // hyprctl -j devices gives JSON devices info with keyboard layouts
    proc.start(QStringLiteral("hyprctl"),
               {QStringLiteral("-j"), QStringLiteral("devices")});
    proc.waitForFinished(3000);

    if (proc.exitCode() != 0) {
        qWarning() << "kblayout: hyprctl devices failed with exit code"
                   << proc.exitCode();
        return;
    }

    QByteArray output = proc.readAllStandardOutput();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(output, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "kblayout: Failed to parse Hyprland JSON:"
                   << parseError.errorString();
        return;
    }

    auto obj = doc.object();
    auto keyboards = obj.value(QStringLiteral("keyboards")).toArray();
    for (const auto &k : keyboards) {
        auto layoutArray = k.toObject().value(QStringLiteral("layouts")).toArray();
        if (!layoutArray.isEmpty()) {
            for (const auto &l : layoutArray) {
                m_layoutSyms.append(l.toString());
            }
            break; // Use first keyboard's layouts
        }
    }

    // Also get current active layout
    QProcess proc2;
    proc2.start(QStringLiteral("hyprctl"),
                {QStringLiteral("-j"), QStringLiteral("keyboard")});
    proc2.waitForFinished(3000);

    if (proc2.exitCode() == 0) {
        auto output2 = proc2.readAllStandardOutput();
        auto doc2 = QJsonDocument::fromJson(output2);
        auto kobj = doc2.object();
        m_activeLayoutName = kobj.value(QStringLiteral("active_keymap")).toString();
    }
}

// ============================================================================
// Labwc / Generic fallback
// ============================================================================

bool WaylandBackend::initLabwc()
{
    qInfo() << "kblayout: Labwc backend initialized (key emulation mode)";

    // For Labwc, we can only emulate the layout-switch key
    // Mark as valid but with limited functionality
    m_layoutSyms << QStringLiteral("us") << QStringLiteral("ru");
    m_layoutNames << QStringLiteral("English") << QStringLiteral("Russian");
    return true;
}

bool WaylandBackend::initWayfire()
{
    qInfo() << "kblayout: Wayfire backend initialized (key emulation mode)";

    // Wayfire uses wf-shell IPC for some things, but layout switching
    // is typically done via key emulation
    m_layoutSyms << QStringLiteral("us") << QStringLiteral("ru");
    m_layoutNames << QStringLiteral("English") << QStringLiteral("Russian");
    return true;
}

bool WaylandBackend::initGeneric()
{
    qWarning() << "kblayout: No supported Wayland compositor found,"
               << "using read-only fallback";

    m_layoutSyms << QStringLiteral("us");
    m_layoutNames << QStringLiteral("English (EN)");
    return true;
}

// ============================================================================
// Public API implementations
// ============================================================================

QStringList WaylandBackend::layouts() const
{
    return m_layoutSyms;
}

int WaylandBackend::currentLayout() const
{
    if (m_compositor == QLatin1String("kwin")) {
        if (!m_kwinLayouts)
            return 0;

        QDBusReply<int> reply = m_kwinLayouts->call(QStringLiteral("getLayout"));
        if (reply.isValid())
            return reply.value();
        return 0;
    }

    // For polling-based compositors, return cached index
    return m_currentIdx;
}

void WaylandBackend::setLayout(int index)
{
    if (index < 0 || index >= m_layoutSyms.size())
        return;

    if (m_compositor == QLatin1String("kwin")) {
        if (!m_kwinLayouts)
            return;

        QDBusReply<void> reply = m_kwinLayouts->call(QStringLiteral("setLayout"), index);
        if (!reply.isValid()) {
            qWarning() << "kblayout: KWin setLayout error:" << reply.error().message();
        }
        return;
    }

    if (m_compositor == QLatin1String("sway")) {
        // Sway: switch layout by cycling to target index
        // Sway doesn't support direct layout set, so we cycle N times
        int cycles = index - m_currentIdx;
        if (cycles < 0)
            cycles += m_layoutSyms.size();
        for (int i = 0; i < cycles; ++i)
            fallbackSwitchViaKeyEmulation();
        return;
    }

    if (m_compositor == QLatin1String("hyprland")) {
        // Hyprland: set layout directly via hyprctl keyword
        QProcess proc;
        proc.start(QStringLiteral("hyprctl"),
                   {QStringLiteral("keyword"),
                    QStringLiteral("xkb_layout %1").arg(m_layoutSyms[index])});
        proc.waitForFinished(2000);
        if (proc.exitCode() != 0) {
            qWarning() << "kblayout: hyprctl keyword xkb_layout failed";
            fallbackSwitchViaKeyEmulation();
        }
        return;
    }

    // Labwc, Wayfire, generic: key emulation
    fallbackSwitchViaKeyEmulation();
}

void WaylandBackend::nextLayout()
{
    if (m_layoutSyms.isEmpty())
        return;

    int next = (m_currentIdx + 1) % m_layoutSyms.size();
    setLayout(next);
}

// ============================================================================
// Slots
// ============================================================================

void WaylandBackend::_on_kwin_layoutChanged(uint index)
{
    Q_UNUSED(index)
    readKWinLayouts();
    emit layoutChanged(currentLayout());
}

void WaylandBackend::_on_poll_timer()
{
    QString prevActive = m_activeLayoutName;

    if (m_compositor == QLatin1String("sway")) {
        readSwayLayouts();
    } else if (m_compositor == QLatin1String("hyprland")) {
        readHyprlandLayouts();
    }

    // Find current layout index by comparing active layout name with known layouts
    int newIdx = 0;
    for (int i = 0; i < m_layoutSyms.size(); ++i) {
        if (m_layoutSyms[i] == m_activeLayoutName) {
            newIdx = i;
            break;
        }
    }

    if (newIdx != m_currentIdx) {
        m_currentIdx = newIdx;
        emit layoutChanged(m_currentIdx);
    }
}

// ============================================================================
// Fallback: key emulation via wtype
// ============================================================================

void WaylandBackend::fallbackSwitchViaKeyEmulation()
{
    // Try wtype first (modern, supports Wayland)
    QProcess proc;
    proc.start(QStringLiteral("wtype"),
               {QStringLiteral("-m"), QStringLiteral("Alt+Shift")});
    proc.waitForFinished(2000);

    if (proc.exitCode() == 0) {
        qInfo() << "kblayout: Layout switched via wtype (Alt+Shift)";
        return;
    }

    // Fallback to xdotool (may not work on all Wayland compositors)
    proc.start(QStringLiteral("xdotool"),
               {QStringLiteral("key"), QStringLiteral("Alt+Shift_L")});
    proc.waitForFinished(2000);

    if (proc.exitCode() == 0) {
        qInfo() << "kblayout: Layout switched via xdotool (Alt+Shift_L)";
        return;
    }

    qWarning() << "kblayout: Neither wtype nor xdotool available for key emulation";
}
