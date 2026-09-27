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

#ifndef KBLAYOUTBACKEND_H
#define KBLAYOUTBACKEND_H

#include <QObject>
#include <QStringList>

/**
 * \brief Abstract base class for keyboard layout backends.
 *
 * Each platform (X11, Wayland) provides its own implementation.
 * The backend reads the current layout, lists available layouts,
 * and can switch between them.
 */
class KbLayoutBackend : public QObject
{
    Q_OBJECT
public:
    explicit KbLayoutBackend(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~KbLayoutBackend() = default;

    /**
     * \brief Returns true if the backend is functional.
     *        Returns false if initialization failed.
     */
    virtual bool isValid() const = 0;

    /**
     * \brief Returns list of available layout codes (e.g. "us", "ru", "de").
     */
    virtual QStringList layouts() const = 0;

    /**
     * \brief Returns index of the current layout.
     */
    virtual int currentLayout() const = 0;

    /**
     * \brief Switch to the layout at the given index.
     */
    virtual void setLayout(int index) = 0;

    /**
     * \brief Switch to the next layout in the list (cycle).
     */
    virtual void nextLayout() = 0;

signals:
    /**
     * \brief Emitted when the active layout changes.
     * \param index New current layout index.
     */
    void layoutChanged(int index);

    /**
     * \brief Emitted when LED states change (Caps/Num/Scroll lock).
     * \param caps CapsLock state
     * \param num NumLock state
     * \param scroll ScrollLock state
     */
    void ledStateChanged(bool caps, bool num, bool scroll);
};

#endif // KBLAYOUTBACKEND_H
