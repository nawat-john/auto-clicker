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
