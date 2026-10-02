# wqs

**W**indows **Q**uick **S**hell - a [Quickshell](https://quickshell.org) port for
Windows, built on Qt Quick / QML.

## What it is (and what it isn't)

wqs is a Windows port of Quickshell: it runs Quickshell-style QML, so a shell is a
`shell.qml` describing the windows it owns - edge-anchored panels (bars), floating
windows, and popups - kept alive by a single long-running process. The QML types
(`PanelWindow`, `FloatingWindow`, an `IpcHandler`-style registry) and the command
line follow Quickshell's, adapted where Windows has no Wayland equivalent.

It is **not** a window manager. wqs draws the shell (bars, widgets, flyouts);
the tiling and stacking of your actual application windows stays with the window
manager - on Windows with a tiling WM such as [komorebi](https://github.com/LGUG2Z/komorebi).
The two are designed to cooperate (see [Komorebi](#komorebi) below).

## Features

- the `Quickshell` and `Quickshell.Io` QML modules, so Quickshell configs run
  unchanged (see [Quickshell types](#quickshell-types))
- `ShellRoot` - the root object, holding the windows plus the `Quickshell`
  singleton (paths, version, log location, live screen list)
- `PanelWindow` - a frameless, always-on-top window docked to a screen edge
  with an `exclusiveZone` (the Windows approximation of a Wayland strut)
- `FloatingWindow` - a frameless, always-on-top window that floats freely
- `Process` / `StdioCollector` - run programs and stream their output in QML
- everything assembled in `qml/shell.qml`, no rebuild needed for layout changes
- file logging to `%USERPROFILE%\.config\wqs\wqs.log`
- per-user only, no admin required

## Build

Requires Qt 6.5+ (developed against 6.8.3, MinGW) and CMake 3.21+.

```powershell
cmake -B build -G Ninja
cmake --build build
```

`build\wqs.exe` is a console application, so its subcommands (`wqs list`,
`wqs ipc`, `wqs log`, ...) behave like any other CLI tool in `cmd`, PowerShell,
fish, and MSYS2/Cygwin shells: they wait for the command to finish, print its
output, and report its exit code. Starting the shell (`wqs`, `wqs -p <path>`, or
`wqs -c <name>`) re-launches the process detached and returns the prompt
immediately, so the bar runs with no console window; its diagnostics go to
`%USERPROFILE%\.config\wqs\wqs.log`. Because the Qt `bin` directories are not on
the system `PATH`, prepend them before running:

```powershell
$env:Path = "C:\Qt\6.8.3\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:Path"
.\build\wqs.exe
```

## Config

The shell is a QML document. wqs looks for it in the same order Quickshell does:

1. `-p, --path <path>` (or `WQS_CONFIG_PATH`) - a `shell.qml` file, or a directory
   containing one
2. `-c, --config <name>` (or `WQS_CONFIG_NAME`) -
   `%USERPROFILE%\.config\wqs\<name>\shell.qml`
3. `%USERPROFILE%\.config\wqs\shell.qml`, then `...\wqs\default\shell.qml`
4. the copy bundled into the executable, so wqs runs even with no config

A user config is read from disk, so layouts can change without a rebuild; the
bundled shell resolves from `qrc:/qt/qml/wqs/qml/shell.qml`. The QML is Quickshell
QML: `import Quickshell` (and `Quickshell.Io` for processes and IPC), then a
`ShellRoot` containing the windows. A snippet:

```qml
import Quickshell
import Quickshell.Io
import QtQuick

ShellRoot {
    id: shell
    property string clockText: ""

    PanelWindow {
        id: bar

        anchors {
            top: true
            left: true
            right: true
        }

        exclusiveZone: 36
        implicitHeight: 36
        color: "#1e1e2e"
        // ... widgets ...
    }

    FloatingWindow {
        id: example

        visible: false
        width: 260
        height: 160
    }

    Process {
        id: clock
        command: ["cmd", "/c", "time", "/t"]
        running: true
        stdout: StdioCollector {
            onStreamFinished: shell.clockText = text.trim()
        }
    }

    IpcHandler {
        target: "example"
        function handle(message) {
            return message
        }
    }
}
```

## Quickshell types

wqs implements the Quickshell QML API on top of Qt Quick, so an existing config
works as-is. The following are available under `import Quickshell`:

- `Quickshell` - the singleton: `screens` (`list<ShellScreen>`, kept in sync with
  the display list), `shellDir` / `shellPath`, `cacheDir` / `configDir` /
  `dataDir` / `stateDir` (and the `*Path` variants), `processId`, `env`,
  `execDetached()`, `reload()` / `reloadCompleted`, `hasVersion()` / `hasQtVersion()`
- `ShellRoot` - the root object; holds the windows and a `settings` object
  (`watchFiles`, `reloadPopup`), and accepts arbitrary children
- `Reloadable` - reload base class, with the `reloadableId` matching hint
- `Scope` - `Reloadable` variant that propagates reloads to its children in order
- `Singleton` - root component for reloadable singletons
- `Variants` - creates non-`Item` instances of a component from a `model` list,
  each with `modelData`, and acts as a reload scope
- `ScriptModel` - list model that diffs a JS array into insert/remove/move/data-change
  operations, so views animate instead of rebuilding; `values`, `objectProp`,
  `comparisonMode`
- `PersistentProperties` - keeps declared properties across a reload
- `LazyLoader` - asynchronous component loader (`loading` / `active` /
  `activeAsync`, `component` / `source`)
- `PanelWindow` - edge-docked window: `anchors`, `margins`, `exclusiveZone`,
  `exclusionMode`, `aboveWindows`, `focusable`, `color`
- `FloatingWindow` - free window: `x` / `y`, `width` / `height`, min/max size,
  `minimized`, `maximized`, `fullscreen`, `parentWindow`
- `PopupWindow` - window positioned relative to a parent via `anchor` /
  `relativeX` / `relativeY`, hidden by default
- `ShellScreen` - one entry from `Quickshell.screens` (name, geometry, scale, ...)
- value types `panelAnchors`, `panelMargins`, `popupAnchorRect`, and the
  `Edges` / `ExclusionMode` enums

And under `import Quickshell.Komorebi` (a wqs extension, not upstream Quickshell):

- `Komorebi` - the window manager singleton: `available`, `monitors`,
  `workspaces`, `toplevels` (plain JS arrays, rebuilt on change),
  `focusedMonitor`, `focusedWorkspace`, `activeToplevel`, plus
  `dispatch(command, args)`, `query(command, args)`, `refresh()` and
  `monitorFor(screen)`
- `KomorebiMonitor` - `name`, `id`, `device`, `size`, `workArea`, `active`,
  `activeWorkspace`, `workspaces`
- `KomorebiWorkspace` - `name`, `index`, `monitorName`, `active`,
  `windowCount`, `toplevels`
- `KomorebiToplevel` - `title`, `exe`, `cls`, `hwnd`, `rect`,
  `workspaceName`, `monitorName`, `active`
- `KomorebiRect` - `x`, `y`, `width`, `height`

State is fed by komorebi's event pipe: wqs hosts a named pipe, launches
`komorebic.exe subscribe-pipe <name>`, and komorebi pushes the full state as
JSON on every change. Events are debounced, so there is no polling. A one-shot
`komorebic.exe state` bootstraps the model if the pipe has not attached yet,
which is also what `refresh()` does.

And under `import Quickshell.Io`:

- `Process` - `command`, `workingDirectory`, `environment`, `clearEnvironment`,
  `stdinEnabled`, `running`, `start()` / `startDetached()`, `write()`,
  `closeStdin()`, `kill()` / `terminate()` / `signal()`, and the
  `started` / `exited` / `errorOccurred` signals
- `StdioCollector` / `SplitParser` / `DataStreamParser` - `stdout` / `stderr`
  parsers with `dataChanged` / `streamFinished`
- `IpcHandler` - `target` plus a `handle(message)` function, reachable with
  `wqs ipc call <target> handle <args>`

Deviations from upstream Quickshell are Windows-driven: `PanelWindow` is a
topmost frameless window rather than a Wayland layer-shell surface, but an
edge-docked panel still reserves its strip by registering as a Windows app-bar,
and a few Qt-only helpers (clipboard, theme icons) are present but partial.

### The `qs:` config namespace

Config files import each other through the `qs:` prefix, exactly as on
upstream Quickshell: `import qs.services` in `<config>/modules/Foo.qml` means
`<config>/services`. The `qs:` scheme is served by the config directory, which
is where `shell.qml` lives.

Qt will not resolve a module from an import path unless a `qmldir` sits next to
it, and configs ship plain `.qml` files in folders instead. So wqs walks the
config tree at startup and synthesizes a `qmldir` for every directory that lacks
one, declaring each `Type.qml` and marking the ones whose file starts with
`pragma Singleton` as singletons. Those qmldirs only exist in memory and are
answered by the network access manager.

The synthesized qmldir is deliberately not written to disk, so a config stays a
plain folder of QML files and does not accumulate generated files. Upstream
instead follows imports lazily and runs its QML preprocessor in the same pass;
wqs walks the whole config root up front instead, and has no preprocessor yet,
so `//@` pragmas and conditional compilation do not work.

## Command line

wqs carries Quickshell's `qs` tool as `wqs`:

```
wqs                     # run the shell
wqs -p <path>           # run a shell.qml file or directory
wqs -c <name>           # run the named config
wqs -n                  # exit if an instance is already running
wqs -v                  # more logging (repeatable)
wqs list                # list running instances
wqs kill                # ask running instances to quit
wqs ipc call <target> <handler> [args...]
wqs log [-f]            # print the log, optionally following it
```

Instances expose an `IpcHandler`-style registry over a per-instance named pipe; the
built-in `wqs` target provides `version` and `screens`.

## Window titles

Every wqs window carries a title that tells any window manager how to treat it:

- `wqs-window` - an obstacle. A window manager is told to ignore it, and an
  edge-docked panel additionally registers as a Windows app-bar (see
  [Komorebi](#komorebi)) so its strip is reserved in the work area like the
  taskbar's
- `wqs-window-dwm` - part of the Windows DWM shell layer; every WM is
  configured to ignore anything with the `-dwm` suffix, and no workspace is
  reserved

The suffix is the mechanism that counteracts any tiling WM (komorebi, GlazeWM,
and others) without wqs knowing which one is running.

## Komorebi

A tiling WM will happily tile a shell's windows along with everything else, so
wqs hides itself from komorebi the same way [yasb](https://github.com/amnweb/yasb)
does:

- **every wqs window is ignored** by komorebi, so nothing from the shell is
  ever tiled. Because both window kinds share the `wqs-window` prefix, a single
  ignore rule covers them
- **edge-docked panels** (`PanelWindow` anchored to a screen edge) register as a
  **Windows app-bar** via `SHAppBarMessage`, the same mechanism the taskbar uses,
  so the bar's strip is carved out of the work area independent of the WM

Reservation follows upstream's rules, on all four edges: `exclusionMode: Auto`
(the default) reserves exactly the window's own size, and only when the panel has
three anchors. `Normal` reserves `exclusiveZone` and accepts one or three. In
both cases the anchors pick the edge, so a panel anchored `left, top, bottom`
reserves the left strip, `top, left, right` reserves the top one, and so on.

The ignore rule lives in komorebi's `applications.json`
(`%USERPROFILE%\.config\komorebi\applications.json`):

```json
"wqs": {
  "ignore": [
    {
      "kind": "Title",
      "id": "wqs-window",
      "matching_strategy": "StartsWith"
    }
  ]
}
```

## Future

- hot reload: the reloadable object graph, singleton registry and `EngineGeneration`
  plumbing are in place, but nothing triggers them yet - a config directory file
  watcher and a real `Quickshell.reload()` still need to be wired up
- a widget set (clock, workspaces, tray, media) and a theme language
- app-bar registration for non-edge `FloatingWindow`s (Windows only docks
  app-bars to an edge, so centered windows cannot reserve space this way)
- port the Caelestia shell on top of this base: the remaining work is
  `Quickshell.Widgets`, the core types still missing (`SystemClock`, `ObjectModel`,
  `ScriptModel`, `Region`, `TransformWatcher`, `ColorQuantizer`, `Retainable`,
  `EasingCurve`, `ElapsedTimer`, `BoundComponent`), the Caelestia C++ plugin, the
  `qs:` config import path that Caelestia singletons rely on, and Windows
  stand-ins for the Linux-only services (UPower, Bluetooth, MPRIS, Pipewire,
  notifications, global shortcuts)

## License

GPL-3.0. The desktop shell configuration being ported to this base,
[Caelestia](https://github.com/caelestia-dots/shell), is GPL-3.0, so this port
carries the same license.
