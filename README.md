# Ultra Auto Clicker & Key Spammer

A high-performance, lightweight **Auto Clicker and Keyboard Spammer** built in **C++20** with native Win32 APIs. Engineered for sub-millisecond precision, multi-point orchestration, and seamless workflow automation.

---

## ⚡ Key Features

- **High-Precision Engine**: Multimedia timer (`timeBeginPeriod(1)`) + hybrid sleep-spin for ultra-accurate intervals and anti-detection jitter ($\pm\text{ms}$).
- **Multi-Target Automation**: Chain multiple mouse actions (Left, Right, Middle, Double Click) and keyboard key presses.
- **Sync & Async Execution**:
  - **Sync (Sequential)**: Executes target actions in series with customizable Before/After delays.
  - **Async (Parallel)**: Spawns independent high-speed threads for each action target.
- **Screen-Capture Style Coordinate Picker (`F8`)**:
  - Automatically hides the app window.
  - Displays a full-screen crosshair (`+`) and real-time coordinate badge `(X, Y)`.
  - Left-click to lock in coordinates; right-click or `ESC` to cancel.
- **Persistent State & Windows Registry Storage**:
  - All configurations, items, and checkbox selection states are saved automatically to `HKEY_CURRENT_USER\Software\UltraAutoClicker`.
- **Global Hotkeys**:
  - `F6`: Start / Stop Toggle
  - `F8`: Position Picker
  - `F7`: Panic / Emergency Stop
- **Modern Clean Light Theme**: White background with crisp typography, smooth rounded buttons, and embedded application icon.

---

## ⌨️ Hotkeys & Controls

| Hotkey | Action | Description |
| :--- | :--- | :--- |
| **`F6`** | **Start / Stop** | Toggle the clicker engine on/off globally |
| **`F8`** | **Pick Position** | Launch crosshair overlay to capture screen coordinates $(X, Y)$ |
| **`F7`** | **Panic Stop** | Immediate emergency kill-switch |
| **`F5` (hold)** | **Record Keys** | In Recorded Keys mode, record until F5 is released |
| **`Double-Click`** | **Edit Target** | Double-click any row in the list to customize settings |

---

## 🚀 Quick Start

### Running the Application
Simply double-click **`clicker.exe`** to launch the standalone program. No external dependencies or runtime installations required.

### Basic Workflow
1. Click **`Pick Pos (F8)`** or press **`F8`** $\rightarrow$ Click anywhere on screen to capture a coordinate.
2. Click **`+ Key Press`** to add keyboard keys (e.g. `E`, `Space`, `F1`, `1`).
3. Set your desired **Clicks Per Minute (CPM)** or Interval.
4. Select **Sync** or **Async** mode.
5. Press **`F6`** to Start/Stop.

### Record and replay keyboard input

1. Click **+ Record Keys** (or select **Recorded Keys (hold F5)**).
2. Focus the target application. Hold **F5**, press your keys, then release **F5**. Each nonempty recording adds a separate **Recorded Keys** action item. Repeat to add more recordings.
3. Use the row's checkbox, **Edit Item**, **Delete**, **Up** and **Down** to enable, rename, remove or reorder recordings. Editing an item keeps its recorded keys and timing; optional Before/After delays apply around the recording.
4. Press **F6** to play/stop. **Recorded Keys** mode plays enabled recordings in list order and skips other action types. **Sync** mixes recordings with clicks and individual key presses in list order; **Async** runs enabled items independently. **Repeat** sets the pass count (per item in Async); **0** loops until stopped. **F7** stops playback or cancels an unfinished recording.

Recording preserves key-downs, key-ups, held keys, auto-repeat events, left/right modifiers, extended keys and the time between events. Keys still held when F5 is released are released at that point during replay. F5 is the recording control and F7 is reserved for emergency stop; neither is recorded. F6 and F8 can be recorded while F5 is held. F5 recording is blocked during playback.

Each recorded item starts with its first key immediately, without the initial wait before that key, and preserves timing through the release of F5. CPM, interval and jitter add no delays to recorded items. Before/After delays are applied only when explicitly set in the item. Async recordings that share keys or modifiers can interfere with each other; use Sync for ordered playback. Windows scheduling and target input handling can still introduce timing variation; exact zero latency is not guaranteed. Replay sends keys to the application currently in focus and cannot bypass Windows input permission restrictions.

Every item's recording is saved with the action list in the Windows Registry and included in exported JSON profiles, so recordings remain independent and profiles contain the complete data. **`recorded_keys.txt` next to `clicker.exe`** is generated automatically after recording and keeps the latest recording as a backup. It is ignored by Git; no template is needed. On upgrading a version 1 configuration, an existing recording file is imported once as an action item. Empty or cancelled recordings add no items. Recording data contains key codes and timings, so avoid sensitive input. Captures and loaded recordings are limited to one million events, with space reserved for final key releases. At the limit, recording ends safely and the UI reports it. The completed-recording queue is capped at 64 until the UI saves it. Replay validates key indices and timestamps before sending input, checks clock arithmetic, and saturates counters instead of wrapping them.

The build output remains **`clicker.exe` in the project root**.

Run `powershell -ExecutionPolicy Bypass -File tests/run.ps1` to test capture, file validation, multiple recording items, profile/Registry persistence, playback order, filtering, Sync/Async execution, cancellation and hidden GUI operations. Tests enable signed-overflow traps (`-ftrapv`) and container checks (`_GLIBCXX_ASSERTIONS`), cover integer/time/index limits, and intercept simulated input with an isolated Registry key, so they do not type into other applications or overwrite app settings.

---

## 🛠️ Build from Source

### Prerequisites
- **GCC / MinGW-w64** (supporting C++20)
- **Windows SDK / Win32 Tools** (`windres.exe`, `g++.exe`)

### Compilation
Run the provided build script in Command Prompt or PowerShell:

```cmd
build.bat
```

This compiles resource files, links Win32 libraries (`comctl32`, `winmm`, `gdi32`, `dwmapi`, `uxtheme`, `advapi32`), and produces an optimized **`clicker.exe`**.

---

## 📁 Project Structure

```
clicker/
├── clicker.exe          # Compiled standalone binary
├── build.bat            # One-click build script
├── make_assets.py       # Asset generation script
├── res/                 # Resource files (Manifest, Icon, Logo)
│   ├── app.manifest
│   ├── app_icon.ico
│   ├── app_logo.bmp
│   └── resource.rc
└── src/                 # C++20 Source Code
    ├── main.cpp         # Application entry point & mutex check
    ├── models.hpp       # Data models & structures
    ├── config.hpp       # Config parser & validation
    ├── engine.hpp/cpp   # High-precision clicker & key engine
    ├── hotkeys.hpp/cpp  # Global Windows hotkey management
    ├── registry.hpp/cpp # Windows Registry persistence layer
    └── app_ui.hpp/cpp   # Win32 GUI, custom controls & picker overlay
```

---

## 📄 License
Open-source under the MIT License.
