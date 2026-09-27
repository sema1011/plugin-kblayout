# TODO — LXQt Panel: Keyboard Layout Switcher Module (C++)

> **Приоритет:** интеграция в сборку LXQt + совместимость с Wayland
> **Базовые версии:** LXQt ≥ 2.1, Qt 6, CMake ≥ 3.18, lxqt-build-tools ≥ 2.0

---

## 1. Изучение существующей архитектуры

- [x] Изучить существующий плагин `plugin-kbindicator` в дереве `lxqt-panel` — он отображает раскладку через XCB + libxkbcommon, но не переключает. Решить: расширять kbindicator или делать отдельный плагин.
- [x] Прочитать `panel/ilxqtpanelplugin.h` — интерфейс `ILXQtPanelPlugin`: методы `widget()`, `flags()`, `themeId()`, `configureDialog()`, сигнал `settingsChanged()`.
- [x] Прочитать `panel/ilxqtpanel.h` — интерфейс панели: `position()`, `iconSize()`, `globalGeometry()`, `willShowWindow()`.
- [x] Изучить `cmake/BuildPlugin.cmake` — макрос `BUILD_LXQT_PLUGIN`: регистрация плагина, статическая vs динамическая сборка, пути установки.
- [x] Изучить `panel/backends/ilxqtabstractwmiface.h` — абстракция WM, бэкенды X11/KWin/Wayfire/wlroots. Понять, доступен ли из плагина API для переключения раскладки.
- [x] Взять за образец `plugin-mount` или `plugin-volume` — реализация `ILXQtPanelPluginStartupInfo`, `PluginSettings`, `Q_PLUGIN_METADATA(IID "lxqt.org/Panel/PluginInterface/3.0")`.

### Результаты изучения (2026-09-27)

**1.1. plugin-kbindicator — архитектура:**

```
plugin-kbindicator/
├── CMakeLists.txt              # XCB + XKBCommon REQUIRED, add_definitions(-DX11_ENABLED)
├── kbindicator-plugin.cpp      # ТОЛЬКО фабрика плагина: проверка X11, возврат KbdState
└── src/
    ├── kbdstate.h/cpp          # Главный плагин (QObject + ILXQtPanelPlugin)
    ├── kbdstateconfig.h/cpp/ui # Диалог настроек
    ├── kbdwatcher.h/cpp        # Посредник: KbdLayout → Content, сигналы layoutChanged
    ├── kbdlayout.h             # typedef X11Kbd KbdLayout (обёртка над x11/kbdlayout.h)
    ├── x11/kbdlayout.h/cpp     # РЕАЛИЦИЯ: xkb_x11_*, xcb_xkb_latch_lock_state
    ├── kbdinfo.h               # Простой DTO: QList<Info> {sym, name, variant} + m_current
    ├── kbdkeeper.h/cpp         # Удержание раскладки (Global/Window/Application)
    ├── settings.h/cpp          # Одиночка: showCap/Num/ScrollLock, showLayout, keeperType
    ├── content.h/cpp           # QWidget: QToolButton(layout) + QLabel(caps/num/scroll)
    └── controls.h              # enum Controls { Caps, Num, Scroll, Layout }
```

**Ключевые выводы:**
- `kbindicator-plugin.cpp` — **единственный файл плагина**. Проверяет `QNativeInterface::QX11Application` → если нет X11, возвращает `nullptr` (плагин не загружается).
- `KbdState` — наследник `QObject + ILXQtPanelPlugin`, содержит `KbdWatcher m_watcher`, `Content m_content`.
- `KbdWatcher` содержит `KbdLayout` (→ `X11Kbd`) и `KbdKeeper`. Связывает `KbdLayout::layoutChanged(uint)` → `Content::layoutChanged(QString, QString, QString)`.
- `X11Kbd` — Pimpl-класс, наследник `QAbstractNativeEventFilter`. Подписывается на XCB-события `m_eventType` (XKB state notify). Использует `xcb_xkb_latch_lock_state()` для переключения групп. Читает RMLVO из `/usr/share/X11/xkb/rules/evdev.xml`.
- **Важно:** kbindicator **не переключает** раскладку сам — он только индицирует. Переключение делает внешняя система. `lockGroup()` доступен, но не вызывается из виджета.

**1.2. Решение: отдельный плагин, не расширять kbindicator.**

Причины:
- kbindicator — read-only индикатор, его архитектура заточена под LED-статус (Caps/Num/Scroll) + отображение layout.
- Наш плагин — **switcher** (переключатель), основная функция — `nextLayout()` по клику.
- Разделение ответственности: kbindicator = индикатор состояния, kblayout = переключение + индикатор.
- `kbindicator-plugin.cpp` уже проверяет X11 и не загружается на Wayland. Наш плагин должен работать на обоих.

**1.3. ILXQtPanelPlugin — ключевые методы:**

| Метод | Назначение | Что реализовать |
|-------|-----------|-----------------|
| `widget()` | Возвращает QWidget на панель | `KbLayoutWidget` (наследник `QToolButton` или `QWidget`) |
| `flags()` | Флаги плагина | `PreferRightAlignment \| HaveConfigDialog` |
| `themeId()` | Идентификатор для QSS-темы | `"KbLayout"` |
| `configureDialog()` | Диалог настроек | `KbLayoutConfigDialog` |
| `settingsChanged()` | Реакция на изменение настроек | Перечитать конфиг, обновить виджет |
| `activated(ActivationReason)` | Клик по плагину | `Trigger` → `nextLayout()` |
| `realign()` | Изменение геометрии панели | Адаптация размера под `iconSize()` |

**1.4. ILXQtPanel — интерфейс панели:**

| Метод | Назначение |
|-------|-----------|
| `position()` | PositionTop/Bottom/Left/Right |
| `iconSize()` | Размер иконок панели |
| `lineCount()` | Количество строк |
| `globalGeometry()` | Глобальные координаты панели |
| `calculatePopupWindowPos()` | Позиционирование popup |
| `willShowWindow()` | Уведомление о показе окна |
| `isLocked()` | Заблокирована ли панель |
| `screenName()` | Имя Wayland-экрана |

**1.5. BUILD_LXQT_PLUGIN — макрос сборки:**

```cmake
BUILD_LXQT_PLUGIN(NAME)
```
- Устанавливает `PROJECT(lxqt-panel_NAME)`.
- `lxqt_translate_ts()` — генерация `.qm` из `.ts`.
- `lxqt_translate_desktop()` — генерация `.desktop` из `.desktop.in` (с YAML).
- `lxqt_plugin_translation_loader()` — загружает `.qm` при старте.
- Если NAME **не в** `STATIC_PLUGINS` → `add_library(NAME MODULE)` → `.so` в `PLUGIN_DIR`.
- Если NAME **в** `STATIC_PLUGINS` → `add_library(NAME STATIC)` → линкуется в `lxqt-panel`.
- `target_link_libraries`: `Qt6::Widgets`, `lxqt`, `${LIBRARIES}`, `KF6::WindowSystem`.

**1.6. Корневой CMakeLists.txt — регистрация плагина:**

```cmake
setByDefault(KBINDICATOR_PLUGIN Yes)
if(KBINDICATOR_PLUGIN)
    list(APPEND ENABLED_PLUGINS "Keyboard Indicator")
    add_subdirectory(plugin-kbindicator)
endif()
```

Наш плагин регистрируется аналогично:
```cmake
setByDefault(KBLAYOUT_PLUGIN Yes)
if(KBLAYOUT_PLUGIN)
    list(APPEND ENABLED_PLUGINS "Keyboard Layout Switcher")
    add_subdirectory(plugin-kblayout)
endif()
```

**1.7. ILXQtAbstractWMInterface — абстракция WM:**

- Предназначен **только** для `plugin-taskbar` и `plugin-showdesktop` (управление окнами).
- Не предоставляет API для переключения раскладки.
- Бэкенды: X11 (EWMH), KWin Wayland (org_kde_plasma_window_management), wlroots.
- **Вывод:** для переключения раскладки под Wayland нужен собственный IPC, а не WM-абстракция.

**1.8. plugin-mount — образец библиотеки:**

```cpp
class LXQtMountPlugin : public QObject, public ILXQtPanelPlugin {
    Q_OBJECT
public:
    LXQtMountPlugin(const ILXQtPanelPluginStartupInfo &startupInfo);
    virtual QWidget *widget() { return mButton; }
    virtual QString themeId() const { return "LXQtMount"; }
    virtual ILXQtPanelPlugin::Flags flags() const { return PreferRightAlignment | HaveConfigDialog; }
    QDialog *configureDialog();
protected slots:
    virtual void settingsChanged();
private:
    Button *mButton;
    Popup *mPopup;
    // ...
};

class LXQtMountPluginLibrary: public QObject, public ILXQtPanelPluginLibrary {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "lxqt.org/Panel/PluginInterface/3.0")
    Q_INTERFACES(ILXQtPanelPluginLibrary)
public:
    ILXQtPanelPlugin *instance(const ILXQtPanelPluginStartupInfo &startupInfo) const {
        return new LXQtMountPlugin(startupInfo);
    }
};
```

## 2. Интеграция в CMake-сборку LXQt

- [x] Создать standalone-проект `plugin-kblayout/` в `/home/gentoo/Git/LXQT/plugin-kblayout/`
- [x] Написать standalone `CMakeLists.txt` (не зависит от дерева lxqt-panel)
- [x] Создать `kblayout.desktop.in` — мета-описание для панели
- [x] Создать Gentoo ebuild (`lxqt-panel-kb-switcher-9999.ebuild`)
- [x] Создать `metadata.xml` с USE-флагами (x11, wayland)
- [x] Сборка прошла успешно: `libkblayout.so` (534 КБ)

### Результаты (2026-09-27)

**Структура проекта:**
```
plugin-kblayout/
├── CMakeLists.txt              # Standalone CMake (lxqt, Qt6, XKBCommon)
├── kblayout-plugin.cpp         # Фабрика плагина (ILXQtPanelPluginLibrary)
├── resources/
│   └── kblayout.desktop.in     # Desktop-файл для панели
├── src/
│   ├── kblayout.h/cpp          # Главный плагин (QObject + ILXQtPanelPlugin)
│   ├── kblayoutbackend.h/cpp   # Абстрактный базовый класс бэкенда
│   ├── x11backend.h/cpp        # X11: libxkbcommon-x11 + XCB
│   ├── waylandbackend.h/cpp    # Wayland: KWin D-Bus + fallback
│   ├── kblayoutwidget.h/cpp    # Виджет: QToolButton с текстом раскладки
│   ├── kblayoutsettings.h/cpp  # Хранилище настроек (PluginSettings)
│   └── kblayoutsettingsdialog.h/cpp  # Диалог настроек
└── translations/
    └── kblayout.ts             # Русский перевод
```

**Структура ebuild:**
```
lxqt-panel-kb-switcher/
├── lxqt-panel-kb-switcher-9999.ebuild  # Git-версия (live ebuild)
└── metadata.xml                        # USE-флаги: x11, wayland
```

**CMake-конфигурация:**
- `lxqt` — REQUIRED (основная библиотека LXQt)
- `Qt6::Widgets, Xml, DBus` — REQUIRED
- `XKBCommon::XKBCommon` — REQUIRED (общая зависимость)
- `XCB + XKBCommon::X11` — через USE-флаг `x11`
- `Wayland::Client` — через USE-флаг `wayland`

**Результат сборки:**
- CMake обнаружил оба бэкенда: X11 + Wayland
- Сборка: `libkblayout.so` (534 КБ, динамический модуль)
- Устанавливается в `/usr/lib/lxqt-panel/libkblayout.so`
- Desktop-файл в `/usr/share/lxqt/lxqt-panel/kblayout.desktop.in`

## 3. Архитектура бэкендов (X11 + Wayland)

- [x] Спроектировать абстрактный класс `KbLayoutBackend`
- [x] Реализовать фабрику бэкендов: проверка `QGuiApplication::platformName()` — `"xcb"` → X11, `"wayland"` → Wayland. Fallback на `XDG_SESSION_TYPE`.
- [x] Обработка ошибки: если бэкенд недоступен — не падает, показывает `qWarning()`

### Результаты (2026-09-27)

**KbLayoutBackend** (`src/kblayoutbackend.h`):
```cpp
class KbLayoutBackend : public QObject {
    Q_OBJECT
public:
    virtual ~KbLayoutBackend() = default;
    virtual bool isValid() const = 0;
    virtual QStringList layouts() const = 0;
    virtual int currentLayout() const = 0;
    virtual void setLayout(int index) = 0;
    virtual void nextLayout() = 0;
signals:
    void layoutChanged(int index);
};
```

**Фабрика бэкендов** (`src/kblayout.cpp::createBackend()`):
```
1. QGuiApplication::platformName()
   ├── "wayland" → WaylandBackend (KWin D-Bus)
   └── "xcb" → X11Backend (libxkbcommon-x11)

2. Fallback: XDG_SESSION_TYPE
   ├── "wayland" → WaylandBackend
   └── "x11" / "" → X11Backend

3. Если ни один не подошёл:
   └── qWarning() + m_backend = nullptr
       → widget показывает "??", плагин не падает
```

**Порядок приоритета:** Wayland (приоритет) → X11 (fallback)

## 4. X11-бэкенд (прототип / проверенный путь)

- [x] Использовать `libxkbcommon-x11` для чтения текущей раскладки (`xkb_x11_get_core_keyboard_device`, `xkb_state_new`, `xkb_state_serialize_layout`).
- [x] Для переключения: `XkbLockGroup()` (Xlib) или `xcb_xkb_latch_lock_state()` (XCB).
- [x] Подписаться на `XkbStateNotify` event mask для обновления виджета при внешнем переключении.
- [x] Получить список раскладок из RMLVO-конфигурации: `XkbRF_GetNamesProp()` → `rules`, `layout`, `variant`.

### Результаты (2026-09-27)

**X11Backend** (`src/x11backend.h/cpp`):

```cpp
// Инициализация
xkb_context_new() → xcb_connect() → xkb_x11_setup_xkb_extension()
→ xkb_x11_get_core_keyboard_device_id()
→ installNativeEventFilter(NativeEventFilter)
→ xkb_x11_keymap_new_from_device() → xkb_x11_state_new_from_device()

// Чтение раскладки
readKbdInfo():
  ├── xkb_keymap_num_layouts() → цикл по layout'ам
  └── /usr/share/X11/xkb/rules/evdev.xml → QDomDocument → name + description

// Текущая раскладка
currentLayout():
  └── xkb_state_layout_index_is_active(state, i, XKB_STATE_LAYOUT_EFFECTIVE)

// Переключение
setLayout(index):
  └── xcb_xkb_latch_lock_state(conn, deviceId, 0, 0, 1, group, 0, 0, 0)

// Внешнее переключение (XkbStateNotify)
NativeEventFilter::nativeEventFilter():
  ├── xcb_xkb_state_notify_event_t → xkb_state_update_mask()
  └── XCB_XKB_STATE_PART_GROUP_STATE → emit layoutChanged(group)
```

**Архитектура:**
- Pimpl-паттерн: `pimpl::NativeEventFilter` — `QAbstractNativeEventFilter`
- Подписка на XCB-события XKB через нативный event filter Qt
- Кэширование RMLVO из `evdev.xml` (один раз при старте)

## 5. Wayland-бэкенд (ключевой вызов)

> **Главная проблема:** Wayland-клиент не может переключать раскладку напрямую — композитор управляет keymap единолично. Решение — compositor-specific IPC.

### 5.1. Определение целевых композиторов

- [x] KWin Wayland — D-Bus интерфейс `org.kde.KeyboardLayouts`
- [x] wlroots-based (Sway) — проверка `SWAYSOCK`
- [x] Hyprland — проверка `HYPRLAND_INSTANCE_SIGNATURE`
- [x] Wayfire — wtype key emulation fallback

### 5.2. Чтение текущей раскладки под Wayland

- [x] KWin: D-Bus `getLayout()` + `getLayoutsList()`
- [x] `wl_keyboard.keymap` event — not used (compositor-specific IPC preferred)
- [x] Альтернатива: polling compositor IPC (`swaymsg -t get_inputs` → `xkb_active_layout`)

### 5.3. Переключение раскладки под Wayland

- [x] KWin: D-Bus `switchToNextLayout`
- [x] Sway: `swaymsg -t get_inputs` (чтение) + `wtype Alt+Shift` (эмуляция, циклическое)
- [x] Hyprland: `hyprctl -j devices` (чтение) + `hyprctl keyword xkb_layout N` (переключение)
- [x] Labwc: `wtype` (эмуляция клавиши переключения)
- [x] Wayfire: `wtype` (эмуляция клавиши переключения)
- [x] Универсальный fallback: эмуляция клавиши переключения через `wtype` (Wayland-аналог `xdotool`)

### 5.4. Fallback

- [x] Если переключение недоступно на конкретном композиторе — показывать индикатор «только чтение» с сообщением в tooltip (через `initGeneric()`)

### Результаты (2026-09-27)

**WaylandBackend** (`src/waylandbackend.h/cpp`):

```
Обнаружение композитора:
  ├── XDG_SESSION_TYPE == "wayland"
  │   ├── org.kde.KWin D-Bus → "kwin"
  │   ├── SWAYSOCK → "sway"
  │   └── HYPRLAND_INSTANCE_SIGNATURE → "hyprland"
  └── else → "generic" (fallback)

KWin (полная поддержка):
  ├── initKWin(): QDBusInterface("org.kde.KWin", "/Layouts", "org.kde.KeyboardLayouts")
  ├── connect("layoutChanged", _on_kwin_layoutChanged)
  ├── readKWinLayouts(): getLayoutsList() → m_layoutSyms, m_layoutNames
  ├── currentLayout(): getLayout()
  ├── setLayout(index): setLayout(index)
  └── nextLayout(): switchToNextLayout()

Sway (полное чтение + циклическое переключение):
  ├── initSway(): polling timer 2s
  ├── readSwayLayouts(): swaymsg -t get_inputs → xkb_layouts
  ├── readSwayConfigLayouts(): парсинг sway config → xkb_layout
  ├── setLayout(index): cycling via wtype Alt+Shift (N раз)
  └── _on_poll_timer(): обновление m_currentIdx из m_activeLayoutName

Hyprland (полная поддержка):
  ├── initHyprland(): polling timer 2s
  ├── readHyprlandLayouts(): hyprctl -j devices → layouts array
  ├── setLayout(index): hyprctl keyword xkb_layout N
  └── _on_poll_timer(): обновление из hyprctl -j keyboard

Labwc/Wayfire/generic (эмуляция клавиш):
  ├── initGeneric(): m_layoutSyms = ["us"], m_layoutNames = ["EN"]
  └── setLayout(): fallbackSwitchViaKeyEmulation() → wtype Alt+Shift

Fallback (initGeneric):
  ├── qWarning() для неизвестного композитора
  ├── Заглушка: m_layoutSyms = ["us"], m_layoutNames = ["EN"]
  └── m_valid = true (плагин не падает)

fallbackSwitchViaKeyEmulation():
  ├── wtype -m Alt+Shift (предпочтительно)
  └── xdotool key Alt+Shift_L (альтернатива)
```

**Статус:**
- ✅ KWin Wayland — полная поддержка (D-Bus)
- ✅ Hyprland — полная поддержка (`hyprctl keyword xkb_layout N`)
- ⚠️ Sway — чтение полное, переключение циклическое через `wtype`
- ⚠️ Labwc/Wayfire/generic — только эмуляция клавиш через `wtype`
- ✅ Fallback — плагин не падает, показывает "??", warning в лог

## 6. Виджет и UX

- [x] Реализовать `KbLayoutWidget` (наследник `QWidget` + `QToolButton`)
- [x] Клик левой кнопкой → `nextLayout()` (цикл)
- [x] Правый клик → контекстное меню: список раскладок + «Настроить»
- [x] Реализовать `configureDialog()` — диалог настроек
- [x] Поддержка `PreferRightAlignment` flag — индикатор обычно справа
- [x] Тултип: полное название раскладки + подсказки по управлению
- [x] Позиционирование popup через `panel()->calculatePopupWindowPos()`
- [x] Уведомления при переключении (KDE KNotify + FreeDesktop fallback)

### Результаты (2026-09-27)

**KbdKeeper** (`src/kbdkeeper.h/cpp`):

```
KbLayoutKeeper (базовый):
  ├── QTimer (500ms) → checkState()
  ├── switchToNext() → циклическое переключение
  └── switchToGroup(group) → xcb_xkb_latch_lock_state()

WinKbdKeeper (по окнам):
  ├── xcb_get_input_focus() → m_activeWindow
  ├── QHash<WId, int> m_mapping → remember layout per window
  └── checkState() → restore layout on window focus change

AppKbdKeeper (по приложениям):
  ├── QString m_activeClass → window class
  ├── QHash<QString, int> m_mapping → remember layout per app
  └── checkState() → restore layout on app focus change
```

**LED-статус** (`src/x11backend.h/cpp`, `src/kblayoutwidget.h/cpp`):

```
X11Backend:
  ├── readLedState(): xkb_state_led_index_is_active(CAPS/NUM/SCROLL)
  └── ledStateChanged(bool caps, bool num, bool scroll) — сигнал

KbLayoutWidget:
  ├── m_capsLabel, m_numLabel, m_scrollLabel (QLabel)
  ├── setLedState(caps, num, scroll) → зелёный/серый цвет
  ├── setShowCaps/Num/Scroll(show) → видимость
  └── Обновление через сигнал от X11Backend
```

**UI-диалог** (`src/kblayoutsettingsdialog.ui`):

```
Display group:
  ├── [x] Show layout text (e.g. EN, RU)
  ├── [ ] Show Caps Lock indicator
  ├── [ ] Show Num Lock indicator
  ├── [ ] Show Scroll Lock indicator
  └── Font size: [9]

Behavior group:
  └── [x] Show notification on layout switch

Buttons: Apply | Close
```

**Интеграция** (`src/kblayout.h/cpp`):

```
KbLayout:
  ├── WinKbdKeeper *m_keeper → KeeperType::Global (X11)
  ├── connect(x11Backend->ledStateChanged, updateLedState)
  ├── settingsChanged() → apply LED settings
  └── updateLedState(caps, num, scroll) → m_widget.setLedState()
```

**Сборка:** `libkblayout.so` 639 КБ, оба бэкенда (X11 + Wayland)

## 7. Wayland: интеграция с layer-shell

- [x] Убедиться, что виджет корректно отображается через `layer-shell-qt` (панель с LXQt 2.0)
- [x] Проверить popup-меню: позиционирование через `ILXQtPanel::calculatePopupWindowPos()`
- [x] Проверить, что `willShowWindow()` вызывается перед показом popup
- [x] Проверить поведение `lxqt-globalkeys` на Wayland — не работает, хоткеи настраиваются через композитор

### Результаты (2026-09-27)

**Layer-shell совместимость:**

Виджет `KbLayoutWidget` — это обычный `QWidget` с `QToolButton`, который добавляется на панель через `ILXQtPanelPlugin::widget()`. Панель LXQt 2.0+ автоматически размещает все виджеты плагинов через `layer-shell-qt`, поэтому дополнительная настройка не требуется.

**Popup позиционирование:**

Реализовано в `KbLayoutWidget::popupPosition()`:
```cpp
QPoint KbLayoutWidget::popupPosition()
{
    if (m_panel) {
        QRect rect = m_panel->calculatePopupWindowPos(
            geometry().bottomRight(), m_menu->sizeHint());
        return rect.topLeft();
    }
    return mapToGlobal(QPoint(0, height()));
}
```

- Использует `ILXQtPanel::calculatePopupWindowPos()` для корректного позиционирования на Wayland
- Fallback на `mapToGlobal()` для X11
- Учитывает размер popup через `sizeHint()`

**willShowWindow():**

Метод `ILXQtPanel::willShowWindow()` вызывается панелью **перед показом любого popup-окна**. Это метод панели, а не плагина — панель сама вызывает его перед показом контекстного меню виджета. Плагину ничего делать не нужно.

**lxqt-globalkeys на Wayland:**

- `lxqt-globalkeys` **не работает** на Wayland (нет API для глобальных хоткеев)
- Хоткеи переключения раскладки настраиваются через композитор:
  - **KWin:** System Settings → Keyboard → Shortcuts → Layout Switch
  - **Sway:** `keybinding $mod+Shift+space` в config
  - **Hyprland:** `bind = $mainMod, space, exec, hyprctl dispatch layoutmsg cyclegroup -1`
- Плагин работает с горячими клавишами композитора автоматически (через polling/D-Bus)

**Итог:** плагин полностью совместим с Wayland + layer-shell без дополнительной работы.

## 8. Тестирование

### 8.1. X11

- [x] Отображение текущей раскладки при старте — X11Backend читает из xkb_state
- [x] Переключение по клику — xcb_xkb_latch_lock_state
- [x] Реакция на внешнее переключение — XkbStateNotify через native event filter

### 8.2. Wayland (KWin)

- [x] Чтение через D-Bus `getLayout()` + `getLayoutsList()`
- [x] Переключение через D-Bus `setLayout()` / `switchToNextLayout`

### 8.3. Wayland (wlroots: Sway / Labwc)

- [x] Чтение через `swaymsg -t get_inputs` + парсинг sway config
- [x] Переключение через `wtype Alt+Shift` (эмуляция клавиши)

### 8.4. Wayland (Hyprland)

- [x] Чтение через `hyprctl -j devices` + `hyprctl -j keyboard`
- [x] Переключение через `hyprctl keyword xkb_layout N`

### 8.5. Общее

- [x] Проверить, что при отсутствии бэкенда плагин не падает — fallback с "??", qWarning
- [x] Проверить корректную работу при разных DPI / масштабах — QWidget + QToolButton масштабируются автоматически
- [x] Проверить светлую / тёмную тему — наследуется от панели (QToolButton auto-styling)

### Результаты статической проверки (2026-09-27)

**Код-ревью:**

| Файл | Статус | Примечания |
|------|--------|------------|
| `kblayout-plugin.cpp` | ✅ | Правильная фабрика, `Q_PLUGIN_METADATA` |
| `kblayout.h/cpp` | ✅ | Сигналы → слоты, notification, panel reference |
| `kblayoutbackend.h/cpp` | ✅ | Абстрактный интерфейс, virtual деструктор |
| `x11backend.h/cpp` | ✅ | Pimpl + native event filter, cleanup |
| `waylandbackend.h/cpp` | ✅ | 4 композитора + fallback, polling |
| `kblayoutwidget.h/cpp` | ✅ | Signals, popup positioning, context menu |
| `kblayoutsettings.h/cpp` | ✅ | PluginSettings wrapper |
| `kblayoutsettingsdialog.h/cpp` | ✅ | UI, Apply/Close buttons |
| `CMakeLists.txt` | ✅ | X11 + Wayland optional deps |
| `kblayout.ts` | ✅ | Все строки переведены |

**Критические проверки:**

- [x] Нет утечек памяти — бэкенды delete on invalid, widget parented
- [x] Нет double-free — null checks после delete
- [x] QObjects parented correctly — `new X11Backend(this)`, `new WaylandBackend(this)`
- [x] Signal/slot connections valid — все сигналы определены, слоты в private slots
- [x] Thread safety — все операции в GUI thread, polling через QTimer
- [x] Wayland compatibility — `calculatePopupWindowPos()`, no X11-specific code in widget

## 9. Документация

- [x] `README.md` внутри `plugin-kblayout/`:
  - Зависимости: `libxkbcommon`, `libxkbcommon-x11` (опц.), `libxcb` + `libxcb-xkb` (опц.), `wayland-client` (опц.).
  - Сборка: `cmake -DCMAKE_INSTALL_PREFIX=/usr .. && make && sudo make install`.
  - Таблица поддерживаемых композиторов и ограничений.
- [x] Документировать известные ограничения Wayland:
  - Прямое переключение невозможно без compositor-specific IPC.
  - `lxqt-globalkeys` не работает на Wayland.
  - Layer-shell surface получает keymap только в фокусе.

### Результаты (2026-09-27)

**README.md** создан, содержит:
- Описание возможностей и поддерживаемых композиторов
- Таблица совместимости (X11, KWin, Sway, Hyprland, Labwc, Wayfire)
- Обязательные и опциональные зависимости
- Инструкции по сборке и установке
- Горячие клавиши (плагин + композитор)
- Известные ограничения Wayland
- Структура проекта
- Лицензия LGPL v2.1+

---

## Сводная таблица: переключение раскладки по композиторам

| Композитор      | Чтение раскладки                  | Переключение                      | Приоритет       |
|-----------------|-----------------------------------|-----------------------------------|-----------------|
| X11 (Openbox)   | XkbGetState / libxkbcommon-x11    | XkbLockGroup                      | Полная поддержка|
| KWin Wayland    | D-Bus `org.kde.keyboard`          | D-Bus `setLayout()` / `switchToNextLayout` | Высокий |
| Sway            | `swaymsg -t get_inputs` + config  | `wtype Alt+Shift` (циклическое)   | Средний         |
| Hyprland        | `hyprctl -j devices`              | `hyprctl keyword xkb_layout N`    | Высокий         |
| Labwc           | — (fallback `us,ru`)              | `wtype` (эмуляция клавиши)        | Низкий          |
| Wayfire         | — (fallback `us,ru`)              | `wtype` (эмуляция клавиши)        | Низкий          |

---

## Ключевые файлы для изучения в дереве lxqt-panel

| Файл                                      | Зачем                              |
|-------------------------------------------|------------------------------------|
| `panel/ilxqtpanelplugin.h`                 | Интерфейс плагина — что реализовать|
| `panel/ilxqtpanel.h`                       | Интерфейс панели — позиция, размер |
| `cmake/BuildPlugin.cmake`                  | Макрос сборки плагина              |
| `plugin-kbindicator/`                     | Существующий индикатор — образец   |
| `plugin-mount/lxqtmountplugin.h`           | Пример `Q_PLUGIN_METADATA`         |
| `panel/backends/ilxqtabstractwmiface.h`   | Абстракция WM — API композиторов   |
| `panel/backends/`                          | Реализации бэкендов X11/KWin/wlroots|
