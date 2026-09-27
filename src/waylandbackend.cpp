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
#include "kblayout-config.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDir>
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

WaylandBackend::WaylandBackend(QObject *parent) :
    KbLayoutBackend(parent)
{
    init();
}

WaylandBackend::~WaylandBackend()
{
    // m_kwinLayouts and m_pollTimer are children of this object,
    // so Qt's parent-child mechanism will automatically delete them.
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

    // 4. Labwc — check if labwc process is running
    if (isProcessRunning(QStringLiteral("labwc"))) {
        m_compositor = QStringLiteral("labwc");
        return initLabwc();
    }

    // 5. Wayfire — check if wayfire process is running
    if (isProcessRunning(QStringLiteral("wayfire"))) {
        m_compositor = QStringLiteral("wayfire");
        return initWayfire();
    }

    // 6. Generic fallback
    m_compositor = QStringLiteral("generic");
    qWarning() << "kblayout: Wayland compositor not detected (session type: wayland)";
    return initGeneric();
}

// ============================================================================
// KWin
// ============================================================================

bool WaylandBackend::initKWin()
{
    // Try org.kde.KeyboardLayouts first
    m_kwinLayouts = new QDBusInterface(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Layouts"),
        QStringLiteral("org.kde.KeyboardLayouts"),
        QDBusConnection::sessionBus(),
        this);

    if (m_kwinLayouts->isValid()) {
        // Connect to layoutChanged signal
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/Layouts"),
            QStringLiteral("org.kde.KeyboardLayouts"),
            QStringLiteral("layoutChanged"),
            this,
            SLOT(_on_kwin_layoutChanged(uint)));

        // Initialize cached layout index from D-Bus
        QDBusReply<int> reply = m_kwinLayouts->call(QStringLiteral("getLayout"));
        if (reply.isValid())
            m_currentLayoutIndex = reply.value();

        // Synchronous read via D-Bus — ensures we have layouts before init returns
        readKWinLayoutsSync();

        // Start LED state polling timer
        m_ledPollTimer = new QTimer(this);
        m_ledPollTimer->setInterval(Kblayout::LedPollIntervalMs);
        connect(m_ledPollTimer, &QTimer::timeout,
                this, &WaylandBackend::_on_led_poll_timer);
        m_ledPollTimer->start();

        // Start async refresh for full layout names (qdbus6 --literal)
        readKWinLayouts();
    }

    // Fallback: try org.freedesktop.Implementations.Keyboards
    if (!m_valid && QDBusConnection::sessionBus().isConnected()) {
        QDBusInterface kbIface(
            QStringLiteral("org.freedesktop.Implementations"),
            QStringLiteral("/org/freedesktop/Implementations/Keyboards"),
            QStringLiteral("org.freedesktop.Implementations.Keyboards"),
            QDBusConnection::sessionBus(),
            this);

        if (kbIface.isValid()) {
            qInfo() << "kblayout: Using org.freedesktop.Implementations.Keyboards";
            QDBusReply<QVariant> reply =
                kbIface.call(QStringLiteral("GetLayoutsList"));
            if (reply.isValid()) {
                m_layoutSyms.clear();
                m_layoutNames.clear();
                // D-Bus returns QList<QList<QVariant>>, which becomes
                // QVariant -> QVariantList -> QList<QVariant>
                QVariantList outer = reply.value().toList();
                for (const auto &outerItem : outer) {
                    QVariantList inner = outerItem.toList();
                    if (inner.size() >= 3) {
                        // [symbol, variant, display_name]
                        m_layoutSyms.append(inner[0].toString());
                        m_layoutNames.append(inner[2].toString());
                    }
                }
                m_valid = !m_layoutSyms.isEmpty();
            }

            // Connect to layoutChanged signal
            QDBusConnection::sessionBus().connect(
                QStringLiteral("org.freedesktop.Implementations"),
                QStringLiteral("/org/freedesktop/Implementations/Keyboards"),
                QStringLiteral("org.freedesktop.Implementations.Keyboards"),
                QStringLiteral("LayoutChanged"),
                this,
                SLOT(_on_kwin_layoutChanged(uint)));
        }
    }

    // Final fallback: try kxkbrc synchronously
    if (!m_valid) {
        qWarning() << "kblayout: D-Bus returned 0 layouts, trying kxkbrc fallback";
        readKXkbConfigSync();
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
    // Async: use qdbus6 command to refresh layout names (non-blocking).
    // Only called after init (via _on_kwin_layoutChanged).
    if (!m_kwinLayouts || !m_kwinLayouts->isValid())
        return;

    if (m_asyncProcess)
        delete m_asyncProcess;
    m_asyncProcess = new QProcess(this);

    m_asyncProcess->start("qdbus6", {
        "--literal",
        "org.kde.KWin", "/Layouts",
        "org.kde.KeyboardLayouts.getLayoutsList"
    });

    connect(m_asyncProcess, &QProcess::finished,
            this, &WaylandBackend::onKWinLayoutsFinished);
}

void WaylandBackend::readKWinLayoutsSync()
{
    // Synchronous D-Bus call — used during init to ensure we have layouts
    // before the backend is returned to the caller.
    if (!m_kwinLayouts || !m_kwinLayouts->isValid())
        return;

    m_layoutSyms.clear();
    m_layoutNames.clear();

    // Call getLayoutsList synchronously via D-Bus
    QDBusReply<QVariant> reply =
        m_kwinLayouts->call(QStringLiteral("getLayoutsList"));

    if (reply.isValid()) {
        // D-Bus returns a nested variant: Argument: a(sss) {...}
        // Strip "Argument: " prefix and parse as JSON-like
        QVariant variant = reply.value();
        QString str = variant.toString();

        // Parse: Argument: a(sss) {[Argument: (sss) "us", "", "English (US)"], ...}
        // Extract all "sym", "", "Display Name" triplets
        QRegularExpression re("\"(\\w+)\",\\s*\"([^\"]*)\",\\s*\"([^\"]*)\"");
        QRegularExpressionMatchIterator it = re.globalMatch(str);

        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString sym = match.captured(1);
            QString displayName = match.captured(3);
            if (!sym.isEmpty()) {
                m_layoutSyms.append(sym);
                m_layoutNames.append(displayName.isEmpty() ? sym.toUpper() : displayName);
            }
        }
    }

    m_valid = !m_layoutSyms.isEmpty();
}

void WaylandBackend::onKWinLayoutsFinished()
{
    if (!m_asyncProcess)
        return;

    if (m_asyncProcess->exitCode() == 0) {
        QByteArray output = m_asyncProcess->readAllStandardOutput();
        QString outputStr = QString::fromUtf8(output).trimmed();
        // Parse qdbus6 --literal output:
        // [Argument: a(sss) {[Argument: (sss) "us", "", "Английская (США)"], [Argument: (sss) "ru", "", "Русская"]}]
        // Extract all "sym", "", "Display Name" triplets
        QRegularExpression re("\"(\\w+)\",\\s*\"([^\"]*)\",\\s*\"([^\"]*)\"");
        QRegularExpressionMatchIterator it = re.globalMatch(outputStr);

        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString sym = match.captured(1);
            QString displayName = match.captured(3);
            if (!sym.isEmpty()) {
                m_layoutSyms.append(sym);
                m_layoutNames.append(displayName.isEmpty() ? sym.toUpper() : displayName);
            }
        }
    } else {
        qWarning() << "kblayout: qdbus6 failed with exit code"
                   << m_asyncProcess->exitCode();
    }

    m_asyncProcess->deleteLater();
    m_asyncProcess = nullptr;
}

void WaylandBackend::readKXkbConfig()
{
    // Async: use kreadconfig6 to read layout list from kxkbrc (non-blocking).
    // Only called after init (via _on_kwin_layoutChanged).
    if (m_asyncProcess)
        delete m_asyncProcess;
    m_asyncProcess = new QProcess(this);

    m_asyncProcess->start("kreadconfig6", {
        "--file", "kxkbrc",
        "--group", "Layout",
        "--key", "LayoutList"
    });

    connect(m_asyncProcess, &QProcess::finished,
            this, &WaylandBackend::onKXkbConfigFinished);
}

void WaylandBackend::readKXkbConfigSync()
{
    // Synchronous read of kxkbrc — used during init fallback.
    QProcess proc;
    proc.start("kreadconfig6", {
        "--file", "kxkbrc",
        "--group", "Layout",
        "--key", "LayoutList"
    });
    proc.waitForFinished(Kblayout::ProcessTimeoutMs);

    if (proc.exitCode() == 0) {
        QString layout = QString::fromUtf8(
            proc.readAllStandardOutput()).trimmed();

        if (layout.isEmpty()) {
            qWarning() << "kblayout: kreadconfig6 returned empty";
            return;
        }

        // Parse comma-separated layout list (e.g. "us,ru,de")
        const auto parts = layout.split(',', Qt::SkipEmptyParts);

        for (const auto &sym : parts) {
            m_layoutSyms.append(sym.trimmed().toLower());
            m_layoutNames.append(sym.trimmed().toUpper());
        }
    } else {
        qWarning() << "kblayout: kreadconfig6 failed with exit code"
                    << proc.exitCode();
    }
}

void WaylandBackend::onKXkbConfigFinished()
{
    if (!m_asyncProcess)
        return;

    QString layout = QString::fromUtf8(
        m_asyncProcess->readAllStandardOutput()).trimmed();
    qDebug() << "kblayout: kreadconfig6 LayoutList =" << layout;

    if (layout.isEmpty()) {
        qWarning() << "kblayout: kreadconfig6 returned empty";
        m_asyncProcess->deleteLater();
        m_asyncProcess = nullptr;
        return;
    }

    // Parse comma-separated layout list (e.g. "us,ru,de")
    const auto parts = layout.split(',', Qt::SkipEmptyParts);

    for (const auto &sym : parts) {
        m_layoutSyms.append(sym.trimmed().toLower());
        m_layoutNames.append(sym.trimmed().toUpper());
    }

    m_asyncProcess->deleteLater();
    m_asyncProcess = nullptr;
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

    // Async: Try swaymsg first (most reliable when keyboard has focus)
    if (m_asyncProcess)
        delete m_asyncProcess;
    m_asyncProcess = new QProcess(this);

    m_asyncProcess->start(QStringLiteral("swaymsg"),
               {QStringLiteral("-t"), QStringLiteral("get_inputs")});

    connect(m_asyncProcess, &QProcess::finished,
            this, &WaylandBackend::onSwayLayoutsFinished);
}

void WaylandBackend::onSwayLayoutsFinished()
{
    if (!m_asyncProcess)
        return;

    if (m_asyncProcess->exitCode() == 0) {
        QByteArray output = m_asyncProcess->readAllStandardOutput();
        parseSwayJson(QString::fromUtf8(output));
        if (!m_layoutSyms.isEmpty()) {
            m_asyncProcess->deleteLater();
            m_asyncProcess = nullptr;
            return;
        }
    }

    // Fallback: read from sway config file
    readSwayConfigLayouts();
    m_asyncProcess->deleteLater();
    m_asyncProcess = nullptr;
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

    // Async: hyprctl -j devices gives JSON devices info with keyboard layouts
    if (m_asyncProcess)
        delete m_asyncProcess;
    m_asyncProcess = new QProcess(this);

    m_asyncProcess->start(QStringLiteral("hyprctl"),
               {QStringLiteral("-j"), QStringLiteral("devices")});

    connect(m_asyncProcess, &QProcess::finished,
            this, &WaylandBackend::onHyprlandLayoutsFinished);
}

void WaylandBackend::onHyprlandLayoutsFinished()
{
    if (!m_asyncProcess)
        return;

    if (m_asyncProcess->exitCode() != 0) {
        qWarning() << "kblayout: hyprctl devices failed with exit code"
                   << m_asyncProcess->exitCode();
        m_asyncProcess->deleteLater();
        m_asyncProcess = nullptr;
        return;
    }

    QByteArray output = m_asyncProcess->readAllStandardOutput();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(output, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "kblayout: Failed to parse Hyprland JSON:"
                   << parseError.errorString();
        m_asyncProcess->deleteLater();
        m_asyncProcess = nullptr;
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

    // Also get current active layout (async)
    if (m_asyncProcess)
        delete m_asyncProcess;
    m_asyncProcess = new QProcess(this);

    m_asyncProcess->start(QStringLiteral("hyprctl"),
               {QStringLiteral("-j"), QStringLiteral("keyboard")});

    connect(m_asyncProcess, &QProcess::finished,
            this, [this]() {
        if (m_asyncProcess && m_asyncProcess->exitCode() == 0) {
            auto output2 = m_asyncProcess->readAllStandardOutput();
            auto doc2 = QJsonDocument::fromJson(output2);
            auto kobj = doc2.object();
            m_activeLayoutName = kobj.value(QStringLiteral("active_keymap")).toString();
        }
        m_asyncProcess->deleteLater();
        m_asyncProcess = nullptr;
    });
}

// ============================================================================
// Labwc / Generic fallback
// ============================================================================

bool WaylandBackend::isProcessRunning(const QString &processName)
{
    // Use pgrep for efficient and secure process detection
    QProcess proc;
    proc.start("pgrep", {"-x", processName});
    proc.waitForFinished(Kblayout::ShortProcessTimeoutMs);
    return proc.exitCode() == 0;
}

QStringList WaylandBackend::parseIniLayout(const QString &filePath,
                                           const QString &section,
                                           const QString &key)
{
    QStringList layouts;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return layouts;

    QTextStream in(&file);
    bool inSection = false;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        // Skip comments and empty lines
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';'))
            continue;

        // Check for section header
        if (line.startsWith('[') && line.endsWith(']')) {
            inSection = (line.mid(1, line.length() - 2) == section);
            continue;
        }

        // If we're in the right section, look for the key
        if (inSection) {
            int eqPos = line.indexOf('=');
            if (eqPos > 0) {
                QString k = line.left(eqPos).trimmed();
                QString v = line.mid(eqPos + 1).trimmed();
                if (k == key) {
                    // Split comma-separated layouts
                    layouts = v.split(',', Qt::SkipEmptyParts);
                    for (auto &l : layouts)
                        l = l.trimmed().toLower();
                    break;
                }
            }
        }
    }
    file.close();
    return layouts;
}

bool WaylandBackend::initLabwc()
{
    qInfo() << "kblayout: Labwc backend initialized";

    // 1. Try to read from ~/.config/labwc/environment (XKB_DEFAULT_LAYOUT)
    QString home = QProcessEnvironment::systemEnvironment().value(QStringLiteral("HOME"));
    QString envFile = QStringLiteral("%1/.config/labwc/environment").arg(home);
    QString xkbLayout;

    QFile env(envFile);
    if (env.open(QIODevice::ReadOnly)) {
        QTextStream in(&env);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith(QStringLiteral("XKB_DEFAULT_LAYOUT="))) {
                xkbLayout = line.mid(21).trimmed().toLower();
                break;
            }
        }
        env.close();
    }

    // 2. Fallback: parse ~/.config/labwc/config for xkb_layout
    if (xkbLayout.isEmpty()) {
        QString configPath = QStringLiteral("%1/.config/labwc/config").arg(home);
        QStringList layouts = parseIniLayout(configPath, QStringLiteral("input"), QStringLiteral("xkb_layout"));
        if (!layouts.isEmpty()) {
            xkbLayout = layouts.join(',');
        }
    }

    // 3. Fallback: read from XKB rules
    if (xkbLayout.isEmpty()) {
        QProcess proc;
        proc.start("setxkbmap", {"-query"});
        proc.waitForFinished(Kblayout::ProcessTimeoutMs);
        if (proc.exitCode() == 0) {
            QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
            QRegularExpression re(QStringLiteral("^layout\\s+=\\s+(\\w+)"));
            auto match = re.match(output);
            if (match.hasMatch()) {
                xkbLayout = match.captured(1).toLower();
            }
        }
    }

    // Populate layouts from XKB rules if we found a layout
    if (!xkbLayout.isEmpty()) {
        m_layoutSyms = xkbLayout.split(',', Qt::SkipEmptyParts);
        for (const auto &sym : m_layoutSyms) {
            m_layoutNames.append(sym.toUpper());
        }
        m_valid = !m_layoutSyms.isEmpty();
        qInfo() << "kblayout: Labwc layouts:" << m_layoutSyms;
    } else {
        qWarning() << "kblayout: Labwc: Could not detect keyboard layout";
        m_layoutSyms << QStringLiteral("us");
        m_layoutNames << QStringLiteral("English");
        m_valid = true;
    }

    return m_valid;
}

bool WaylandBackend::initWayfire()
{
    qInfo() << "kblayout: Wayfire backend initialized";

    // 1. Try to read from ~/.config/wayfire.ini [input] section
    QString home = QProcessEnvironment::systemEnvironment().value(QStringLiteral("HOME"));
    QString wayfireIni = QStringLiteral("%1/.config/wayfire.ini").arg(home);

    QStringList layouts = parseIniLayout(wayfireIni, QStringLiteral("input"), QStringLiteral("xkb_layout"));
    if (!layouts.isEmpty()) {
        m_layoutSyms = layouts;
        for (const auto &sym : m_layoutSyms) {
            m_layoutNames.append(sym.toUpper());
        }
        m_valid = true;
        qInfo() << "kblayout: Wayfire layouts from wayfire.ini:" << m_layoutSyms;
    } else {
        // 2. Fallback: read from XKB rules
        QProcess proc;
        proc.start("setxkbmap", {"-query"});
        proc.waitForFinished(Kblayout::ProcessTimeoutMs);
        if (proc.exitCode() == 0) {
            QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
            QRegularExpression re(QStringLiteral("^layout\\s +=\\s+(\\w+)"));
            auto match = re.match(output);
            if (match.hasMatch()) {
                m_layoutSyms = match.captured(1).toLower().split(',', Qt::SkipEmptyParts);
                for (const auto &sym : m_layoutSyms) {
                    m_layoutNames.append(sym.toUpper());
                }
                m_valid = !m_layoutSyms.isEmpty();
            }
        }

        if (!m_valid) {
            qWarning() << "kblayout: Wayfire: Could not detect keyboard layout";
            m_layoutSyms << QStringLiteral("us");
            m_layoutNames << QStringLiteral("English");
            m_valid = true;
        }
    }

    return m_valid;
}

bool WaylandBackend::initGeneric()
{
    qWarning() << "kblayout: No supported Wayland compositor found,"
               << "using read-only fallback";

    // 1. Try setxkbmap -query (works if XKB is configured)
    QProcess proc;
    proc.start("setxkbmap", {"-query"});
    proc.waitForFinished(Kblayout::ProcessTimeoutMs);

    if (proc.exitCode() == 0) {
        QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        QRegularExpression re(QStringLiteral("^layout\\s +=\\s+(\\w+)"));
        auto match = re.match(output);
        if (match.hasMatch()) {
            m_layoutSyms = match.captured(1).toLower().split(',', Qt::SkipEmptyParts);
            for (const auto &sym : m_layoutSyms) {
                m_layoutNames.append(sym.toUpper());
            }
            m_valid = !m_layoutSyms.isEmpty();
            qInfo() << "kblayout: Generic fallback layouts from setxkbmap:" << m_layoutSyms;
            return m_valid;
        }
    }

    // 2. Fallback: try XKB_DEFAULT_LAYOUT env var
    QString xkbLayout = qEnvironmentVariable("XKB_DEFAULT_LAYOUT");
    if (!xkbLayout.isEmpty()) {
        m_layoutSyms = xkbLayout.toLower().split(',', Qt::SkipEmptyParts);
        for (const auto &sym : m_layoutSyms) {
            m_layoutNames.append(sym.toUpper());
        }
        m_valid = !m_layoutSyms.isEmpty();
        qInfo() << "kblayout: Generic fallback from XKB_DEFAULT_LAYOUT:" << m_layoutSyms;
        return m_valid;
    }

    // 3. Ultimate fallback
    qWarning() << "kblayout: Could not detect any keyboard layout";
    m_layoutSyms << QStringLiteral("us");
    m_layoutNames << QStringLiteral("English (EN)");
    m_valid = true;
    return m_valid;
}

QStringList WaylandBackend::layouts() const
{
    return m_layoutSyms;
}

QStringList WaylandBackend::layoutNames() const
{
    return m_layoutNames;
}

int WaylandBackend::currentLayout() const
{
    if (m_compositor == QLatin1String("kwin")) {
        // Use cached index from D-Bus signal (non-blocking)
        return m_currentLayoutIndex;
    }

    // For polling-based compositors, return cached index
    return m_currentLayoutIndex;
}

void WaylandBackend::setLayout(int index)
{
    if (index < 0 || index >= m_layoutSyms.size())
        return;

    if (m_compositor == QLatin1String("kwin")) {
        // Async: Use qdbus6 command to avoid blocking the main thread
        if (m_asyncProcess)
            delete m_asyncProcess;
        m_asyncProcess = new QProcess(this);
        ++m_asyncProcessGen;
        m_asyncSetLayoutTarget = index;

        m_asyncProcess->start("qdbus6", {
            "org.kde.KWin", "/Layouts",
            "org.kde.KeyboardLayouts.setLayout",
            QString::number(index)
        });

        connect(m_asyncProcess, &QProcess::finished,
                this, &WaylandBackend::onSetLayoutFinished);
        return;
    }

    if (m_compositor == QLatin1String("sway")) {
        // Sway: switch layout by cycling to target index
        // Sway doesn't support direct layout set, so we cycle N times
        int cycles = index - m_currentLayoutIndex;
        if (cycles < 0)
            cycles += m_layoutSyms.size();
        for (int i = 0; i < cycles; ++i)
            fallbackSwitchViaKeyEmulation();
        return;
    }

    if (m_compositor == QLatin1String("hyprland")) {
        // Async: Hyprland: set layout directly via hyprctl keyword
        if (m_asyncProcess)
            delete m_asyncProcess;
        m_asyncProcess = new QProcess(this);
        ++m_asyncProcessGen;
        m_asyncSetLayoutTarget = index;

        m_asyncProcess->start(QStringLiteral("hyprctl"),
                   {QStringLiteral("keyword"),
                    QStringLiteral("xkb_layout %1").arg(m_layoutSyms[index])});

        connect(m_asyncProcess, &QProcess::finished,
                this, &WaylandBackend::onSetLayoutFinished);
        return;
    }

    // Labwc, Wayfire, generic: key emulation
    fallbackSwitchViaKeyEmulation();
}

void WaylandBackend::onSetLayoutFinished()
{
    // Capture the process pointer before any access.
    // If setLayout() was called again, the old process was deleted and
    // m_asyncProcess now points to a new process. Using QPointer ensures
    // we don't access a deleted object.
    QPointer<QProcess> proc = m_asyncProcess;
    if (!proc)
        return;

    // Race condition guard: verify this process belongs to the current generation.
    // If setLayout() was called again, m_asyncProcessGen was incremented and
    // a new process was created. This slot belongs to the old process.
    int currentGen = m_asyncProcessGen;

    if (m_asyncSetLayoutTarget >= 0) {
        m_currentLayoutIndex = m_asyncSetLayoutTarget;
    }

    if (m_compositor == QLatin1String("hyprland") && proc->exitCode() != 0) {
        qWarning() << "kblayout: hyprctl keyword xkb_layout failed";
        fallbackSwitchViaKeyEmulation();
    }

    // Only clean up if this is still the active process
    if (m_asyncProcessGen == currentGen) {
        m_asyncProcess->deleteLater();
        m_asyncProcess = nullptr;
        m_asyncSetLayoutTarget = -1;
    }
}

void WaylandBackend::nextLayout()
{
    if (m_layoutSyms.isEmpty())
        return;

    int next = (m_currentLayoutIndex + 1) % m_layoutSyms.size();
    setLayout(next);
}

// ============================================================================
// Slots
// ============================================================================

void WaylandBackend::_on_kwin_layoutChanged(uint index)
{
    // Cache the index from D-Bus signal (avoids blocking D-Bus call)
    m_currentLayoutIndex = static_cast<int>(index);
    readKWinLayouts();
    emit layoutChanged(m_currentLayoutIndex);
}

void WaylandBackend::_on_led_poll_timer()
{
    // Poll LED states from sysfs
    readLedStatesFromXkb();
}

void WaylandBackend::readLedStatesFromXkb()
{
    // Read LED states from various sources
    bool caps = false, num = false, scroll = false;

    // Try multiple possible LED paths for each LED
    QStringList capsDirs = {
        "/sys/class/leds/input5::capslock",
        "/sys/class/leds/input4::capslock",
        "/sys/class/leds/input3::capslock",
        "/sys/class/leds/input2::capslock",
        "/sys/class/leds/input1::capslock",
        "/sys/class/leds/input0::capslock",
    };

    for (const auto &dir : capsDirs) {
        if (readLedStateFromFile(dir + "/brightness")) {
            caps = true;
            break;
        }
    }

    QStringList numDirs = {
        "/sys/class/leds/input5::numlock",
        "/sys/class/leds/input4::numlock",
        "/sys/class/leds/input3::numlock",
        "/sys/class/leds/input2::numlock",
        "/sys/class/leds/input1::numlock",
        "/sys/class/leds/input0::numlock",
    };

    for (const auto &dir : numDirs) {
        if (readLedStateFromFile(dir + "/brightness")) {
            num = true;
            break;
        }
    }

    QStringList scrollDirs = {
        "/sys/class/leds/input5::scrolllock",
        "/sys/class/leds/input4::scrolllock",
        "/sys/class/leds/input3::scrolllock",
        "/sys/class/leds/input2::scrolllock",
        "/sys/class/leds/input1::scrolllock",
        "/sys/class/leds/input0::scrolllock",
    };

    for (const auto &dir : scrollDirs) {
        if (readLedStateFromFile(dir + "/brightness")) {
            scroll = true;
            break;
        }
    }

    // Only emit if state changed
    if (caps != m_ledCaps || num != m_ledNum || scroll != m_ledScroll) {
        m_ledCaps = caps;
        m_ledNum = num;
        m_ledScroll = scroll;
        emit ledStateChanged(caps, num, scroll);
    }
}

void WaylandBackend::emitInitialLedState()
{
    // Read current LED states and emit immediately (ignoring cached values)
    bool caps = false, num = false, scroll = false;

    // Try multiple possible LED paths for each LED
    QStringList capsDirs = {
        "/sys/class/leds/input5::capslock",
        "/sys/class/leds/input4::capslock",
        "/sys/class/leds/input3::capslock",
        "/sys/class/leds/input2::capslock",
        "/sys/class/leds/input1::capslock",
        "/sys/class/leds/input0::capslock",
    };

    for (const auto &dir : capsDirs) {
        if (readLedStateFromFile(dir + "/brightness")) {
            caps = true;
            break;
        }
    }

    QStringList numDirs = {
        "/sys/class/leds/input5::numlock",
        "/sys/class/leds/input4::numlock",
        "/sys/class/leds/input3::numlock",
        "/sys/class/leds/input2::numlock",
        "/sys/class/leds/input1::numlock",
        "/sys/class/leds/input0::numlock",
    };

    for (const auto &dir : numDirs) {
        if (readLedStateFromFile(dir + "/brightness")) {
            num = true;
            break;
        }
    }

    QStringList scrollDirs = {
        "/sys/class/leds/input5::scrolllock",
        "/sys/class/leds/input4::scrolllock",
        "/sys/class/leds/input3::scrolllock",
        "/sys/class/leds/input2::scrolllock",
        "/sys/class/leds/input1::scrolllock",
        "/sys/class/leds/input0::scrolllock",
    };

    for (const auto &dir : scrollDirs) {
        if (readLedStateFromFile(dir + "/brightness")) {
            scroll = true;
            break;
        }
    }

    // Update cached values and emit immediately
    m_ledCaps = caps;
    m_ledNum = num;
    m_ledScroll = scroll;
    emit ledStateChanged(caps, num, scroll);
}

bool WaylandBackend::readLedStateFromFile(const QString &path)
{
    QFile file(path);
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QString state = QString::fromUtf8(file.readAll()).trimmed();
        file.close();
        return state.toInt() > 0;
    }
    file.close();
    return false;
}

void WaylandBackend::_on_poll_timer()
{
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

    if (newIdx != m_currentLayoutIndex) {
        m_currentLayoutIndex = newIdx;
        emit layoutChanged(m_currentLayoutIndex);
    }
}

// ============================================================================
// Fallback: key emulation via wtype
// ============================================================================

void WaylandBackend::fallbackSwitchViaKeyEmulation()
{
    // Try wtype first (modern, supports Wayland)
    if (m_asyncProcess)
        delete m_asyncProcess;
    m_asyncProcess = new QProcess(this);

    m_asyncProcess->start(QStringLiteral("wtype"),
               {QStringLiteral("-m"), QStringLiteral("Alt+Shift")});

    connect(m_asyncProcess, &QProcess::finished,
            this, [this]() {
        if (m_asyncProcess && m_asyncProcess->exitCode() == 0) {
            qInfo() << "kblayout: Layout switched via wtype (Alt+Shift)";
            m_asyncProcess->deleteLater();
            m_asyncProcess = nullptr;
            return;
        }

        // Fallback to xdotool (may not work on all Wayland compositors)
        if (m_asyncProcess) {
            m_asyncProcess->start(QStringLiteral("xdotool"),
                       {QStringLiteral("key"), QStringLiteral("Alt+Shift_L")});

            connect(m_asyncProcess, &QProcess::finished,
                    this, [this]() {
                if (m_asyncProcess && m_asyncProcess->exitCode() == 0) {
                    qInfo() << "kblayout: Layout switched via xdotool (Alt+Shift_L)";
                } else {
                    qWarning() << "kblayout: Neither wtype nor xdotool available for key emulation";
                }
                m_asyncProcess->deleteLater();
                m_asyncProcess = nullptr;
            });
        }
    });
}
