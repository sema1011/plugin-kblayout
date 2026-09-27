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

#include "x11backend.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QFile>
#include <QDomDocument>
#include <QDebug>

#include <xkbcommon/xkbcommon-x11.h>
#include <xcb/xcb.h>

#define explicit _explicit
#include <xcb/xkb.h>
#undef explicit

namespace pimpl {

class NativeEventFilter : public QAbstractNativeEventFilter
{
public:
    explicit NativeEventFilter(X11Backend *backend) : m_backend(backend) {}

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) override
    {
        if (eventType != "xcb_generic_event_t")
            return false;

        xcb_generic_event_t *event = static_cast<xcb_generic_event_t*>(message);
        if (m_backend->m_eventType == 0)
            return false;

        if ((event->response_type & ~0x80) == m_backend->m_eventType) {
            xcb_xkb_state_notify_event_t *sevent =
                reinterpret_cast<xcb_xkb_state_notify_event_t*>(event);

            if (sevent->xkbType == XCB_XKB_STATE_NOTIFY) {
                xkb_state_update_mask(static_cast<xkb_state*>(m_backend->m_state),
                    sevent->baseMods,
                    sevent->latchedMods,
                    sevent->lockedMods,
                    sevent->baseGroup,
                    sevent->latchedGroup,
                    sevent->lockedGroup);

                if (sevent->changed & XCB_XKB_STATE_PART_GROUP_STATE) {
                    emit m_backend->layoutChanged(sevent->group);
                    return true;
                }
            }
            else if (sevent->xkbType == XCB_XKB_NEW_KEYBOARD_NOTIFY) {
                m_backend->readState();
            }
        }
        return false;
    }

private:
    X11Backend *m_backend;
};

} // namespace pimpl

X11Backend::X11Backend(QObject *parent) :
    KbLayoutBackend(parent)
{
    m_valid = init();
}

X11Backend::~X11Backend()
{
    if (m_eventFilter)
        qApp->removeNativeEventFilter(m_eventFilter);

    if (m_state)
        xkb_state_unref(static_cast<xkb_state*>(m_state));
    if (m_keymap)
        xkb_keymap_unref(static_cast<xkb_keymap*>(m_keymap));
    if (m_connection)
        xcb_disconnect(static_cast<xcb_connection_t*>(m_connection));
    if (m_context)
        xkb_context_unref(static_cast<xkb_context*>(m_context));
}

bool X11Backend::init()
{
    m_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!m_context) {
        qWarning() << "kblayout: Cannot create xkb context";
        return false;
    }

    m_connection = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(static_cast<xcb_connection_t*>(m_connection))) {
        qWarning() << "kblayout: Cannot connect to X server";
        return false;
    }

    uint16_t eventMask = 0;
    if (!xkb_x11_setup_xkb_extension(static_cast<xcb_connection_t*>(m_connection),
                                      XKB_X11_MIN_MAJOR_XKB_VERSION,
                                      XKB_X11_MIN_MINOR_XKB_VERSION,
                                      (xkb_x11_setup_xkb_extension_flags)0,
                                      nullptr, &eventMask,
                                      &m_eventType, nullptr)) {
        qWarning() << "kblayout: XKB extension failed";
        return false;
    }

    m_deviceId = xkb_x11_get_core_keyboard_device_id(static_cast<xcb_connection_t*>(m_connection));
    if (m_deviceId < 0) {
        qWarning() << "kblayout: No XKB device found";
        return false;
    }

    m_eventFilter = new pimpl::NativeEventFilter(this);
    qApp->installNativeEventFilter(m_eventFilter);

    readState();
    return true;
}

void X11Backend::readState()
{
    if (m_keymap)
        xkb_keymap_unref(static_cast<xkb_keymap*>(m_keymap));

    m_keymap = xkb_x11_keymap_new_from_device(static_cast<xkb_context*>(m_context),
                                               static_cast<xcb_connection_t*>(m_connection),
                                               m_deviceId,
                                               (xkb_keymap_compile_flags)0);
    if (!m_keymap) {
        qWarning() << "kblayout: Cannot create XKB keymap";
        return;
    }

    if (m_state)
        xkb_state_unref(static_cast<xkb_state*>(m_state));

    m_state = xkb_x11_state_new_from_device(static_cast<xkb_keymap*>(m_keymap),
                                             static_cast<xcb_connection_t*>(m_connection),
                                             m_deviceId);
    if (!m_state) {
        qWarning() << "kblayout: Cannot create XKB state";
        return;
    }

    readKbdInfo();
}

void X11Backend::readKbdInfo()
{
    m_layoutSyms.clear();
    m_layoutNames.clear();

    xkb_layout_index_t count = xkb_keymap_num_layouts(static_cast<xkb_keymap*>(m_keymap));

    // Read layout names from evdev.xml once
    static bool cached = false;
    static QHash<QString, QString> langCache;
    if (!cached) {
        cached = true;
        QString xmlPath = QStringLiteral("/usr/share/X11/xkb/rules/evdev.xml");
        QFile file(xmlPath);
        if (file.open(QIODevice::ReadOnly)) {
            QDomDocument doc;
            if (doc.setContent(&file)) {
                QDomElement root = doc.documentElement();
                QDomElement layoutList = root.firstChildElement(QStringLiteral("layoutList"));
                for (int i = 0; i < layoutList.childNodes().count(); ++i) {
                    QDomElement config = layoutList.childNodes().at(i)
                        .firstChildElement(QStringLiteral("configItem"));
                    QString desc = config.firstChildElement(QStringLiteral("description"))
                        .firstChild().toText().data();
                    QString name = config.firstChildElement(QStringLiteral("name"))
                        .firstChild().toText().data();
                    langCache.insert(name, desc);

                    QDomElement variantList = layoutList.childNodes().at(i)
                        .firstChildElement(QStringLiteral("variantList"));
                    for (int j = 0; j < variantList.childNodes().count(); ++j) {
                        QDomElement varConfig = variantList.childNodes().at(j)
                            .firstChildElement(QStringLiteral("configItem"));
                        QString varName = varConfig.firstChildElement(QStringLiteral("name"))
                            .firstChild().toText().data();
                        langCache.insert(varName, desc);
                    }
                }
            }
            file.close();
        }
    }

    for (xkb_layout_index_t i = 0; i < count; ++i) {
        const char *name = xkb_keymap_layout_get_name(static_cast<xkb_keymap*>(m_keymap), i);
        if (name) {
            QString sym = QString::fromUtf8(name);
            m_layoutSyms.append(sym);

            auto it = langCache.find(sym);
            if (it != langCache.end()) {
                m_layoutNames.append(it.value());
            } else {
                m_layoutNames.append(sym.toUpper());
            }
        }
    }
}

QStringList X11Backend::layouts() const
{
    return m_layoutSyms;
}

int X11Backend::currentLayout() const
{
    if (!m_state || !m_keymap)
        return 0;

    xkb_layout_index_t count = xkb_keymap_num_layouts(static_cast<xkb_keymap*>(m_keymap));
    for (xkb_layout_index_t i = 0; i < count; ++i) {
        if (xkb_state_layout_index_is_active(static_cast<xkb_state*>(m_state),
                                              i, XKB_STATE_LAYOUT_EFFECTIVE)) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

void X11Backend::setLayout(int index)
{
    if (!m_state || index < 0 || index >= static_cast<int>(m_layoutSyms.size()))
        return;

    xcb_void_cookie_t cookie = xcb_xkb_latch_lock_state(
        static_cast<xcb_connection_t*>(m_connection),
        static_cast<xcb_xkb_device_spec_t>(m_deviceId),
        0, 0,  // affectMods, latchedMods
        1, static_cast<uint8_t>(index),  // lockedMods, group
        0, 0,  // keys, otherState
        0);  // otherLatchedMods

    xcb_generic_error_t *error = xcb_request_check(
        static_cast<xcb_connection_t*>(m_connection), cookie);
    if (error) {
        qWarning() << "kblayout: setLayout error:" << error->error_code;
        free(error);
    }
}

void X11Backend::nextLayout()
{
    if (m_layoutSyms.isEmpty())
        return;

    int current = currentLayout();
    int next = (current + 1) % m_layoutSyms.size();
    setLayout(next);
}
