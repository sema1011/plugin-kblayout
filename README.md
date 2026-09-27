# LXQt Panel: Keyboard Layout Switcher Plugin

Легковесный плагин для панели LXQt с переключением раскладки клавиатуры.
Поддерживает X11 и Wayland (KWin, Sway, Hyprland).

## Возможности

- Отображение текущей раскладки на панели
- Переключение по левому клику (циклическое)
- Иконки-флаги для раскладок (настраиваемый путь к каталогу)
- Переключатель отображения флагов (вкл/выкл)
- Индикаторы CapsLock, NumLock, ScrollLock
- Уведомления при смене раскладки
- Настройки через диалог (размер шрифта, флаги, LED, уведомления)
- Кнопка "Configure layouts..." (открывает lxqt-config-input)
- Поддержка X11 и Wayland

## Поддерживаемые композиторы

| Композитор      | Чтение раскладки                  | Переключение                      | Статус       |
|-----------------|-----------------------------------|-----------------------------------|--------------|
| X11 (Openbox)   | libxkbcommon-x11 + XCB            | xcb_xkb_latch_lock_state          | Полная       |
| KWin Wayland    | D-Bus `org.kde.KeyboardLayouts`   | `qdbus6` (async)                  | Полная       |
| Sway            | `swaymsg -t get_inputs` + config  | `wtype Alt+Shift` (циклическое)   | Частичная    |
| Hyprland        | `hyprctl -j devices`              | `hyprctl keyword xkb_layout N`    | Полная       |
| Labwc           | — (fallback `us,ru`)              | `wtype` (эмуляция клавиши)        | Эмуляция     |
| Wayfire         | — (fallback `us,ru`)              | `wtype` (эмуляция клавиши)        | Эмуляция     |
| Generic         | — (fallback `us`)                 | `wtype` / `xdotool`               | Эмуляция     |

## Зависимости

### Обязательные
- `Qt6` (Widgets, Xml, DBus)
- `lxqt` (liblxqt)
- `libxkbcommon`

### Опциональные
- `libxkbcommon-x11` + `libxcb` + `libxcb-xkb` — X11 бэкенд
- `wayland-client` — Wayland бэкенд
- `wtype` — эмуляция клавиш на Wayland (Labwc, Wayfire, generic)
- `qdbus6` — D-Bus CLI для KWin Wayland

## Сборка

```bash
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr
make
sudo make install
```

### CMake-опции

| Опция | По умолчанию | Описание |
|-------|-------------|----------|
| `KBLAYOUT_X11` | Auto | Включить X11 бэкенд |
| `KBLAYOUT_WAYLAND` | Auto | Включить Wayland бэкенд |

## Установка

После сборки скопируйте плагин в директорию плагинов панели:

```bash
sudo cp lxqt-panel/libkblayout.so /usr/lib/lxqt-panel/
```

Активируйте плагин:
1. Правый клик на панели → «Настроить панель»
2. Добавить → «Keyboard Layout Switcher»
3. Настроить через правый клик на индикаторе

## Настройки

### Отображение
- **Показывать текст** — отображение кода раскладки (EN, RU)
- **Иконки-флаги** — переключатель отображения флагов
- **Паттерн иконок** — путь к каталогу с флагами или паттерн с `%1`
  - Каталог: `/usr/share/sddm/flags` → автоматически `/usr/share/sddm/flags/us.png`
  - Паттерн: `/usr/share/sddm/flags/%1.png` → `/usr/share/sddm/flags/us.png`
- **Размер шрифта** — размер текста на индикаторе
- **Caps Lock / Num Lock / Scroll Lock** — индикаторы LED-статусов

### Поведение
- **Показывать уведомления** — уведомления при смене раскладки
- **Configure layouts...** — открывает `lxqt-config-input --show-page Keyboard Layout`

## Горячие клавиши

### В плагине
- **Левый клик** — переключить на следующую раскладку
- **Правый клик** — открыть меню настроек

### Системные хоткеи
> **Важно:** `lxqt-globalkeys` не работает на Wayland. Настройте хоткеи через композитор:
> - **KWin:** System Settings → Keyboard → Shortcuts → Layout Switch
> - **Sway:** `keybinding $mod+Shift+space` в `~/.config/sway/config`
> - **Hyprland:** `bind = $mainMod, space, exec, hyprctl dispatch layoutmsg cyclegroup -1`

## Известные ограничения

### Wayland
- Прямое переключение раскладки невозможно без compositor-specific IPC
- `lxqt-globalkeys` не работает на Wayland
- Layer-shell surface получает keymap только в фокусе
- На Sway переключение циклическое (через эмуляцию клавиш)
- На Labwc/Wayfire нет чтения текущей раскладки
- LED-индикаторы (Caps/Num/Scroll) читаются из `/sys/class/leds/*/brightness`

### X11
- Переключение работает только для активного XKB-устройства

## Структура проекта

```
plugin-kblayout/
├── CMakeLists.txt              # CMake-конфигурация
├── kblayout-plugin.cpp         # Фабрика плагина (ILXQtPanelPluginLibrary)
├── resources/
│   └── kblayout.desktop.in     # Desktop-описание для панели
├── src/
│   ├── kblayout.h/cpp          # Главный плагин
│   ├── kblayoutbackend.h/cpp   # Абстрактный бэкенд
│   ├── x11backend.h/cpp        # X11: libxkbcommon-x11 + XCB
│   ├── waylandbackend.h/cpp    # Wayland: KWin/Sway/Hyprland
│   ├── kblayoutwidget.h/cpp    # Виджет на панели
│   ├── kblayoutsettings.h/cpp  # Хранение настроек
│   └── kblayoutsettingsdialog.h/cpp  # Диалог настроек
└── translations/
    └── kblayout.ts             # Русский перевод
```

## Лицензия

LGPL v2.1+ — см. файл `kblayout-plugin.cpp` (BEGIN_COMMON_COPYRIGHT_HEADER)
