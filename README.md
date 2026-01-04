
# Hardware Monitor Widget

[![GitHub stars](https://img.shields.io/github/stars/FodiYes/HardwareMonitor?style=social)](https://github.com/FodiYes/HardwareMonitor/stargazers)
[![GitHub license](https://img.shields.io/github/license/FodiYes/HardwareMonitor)](https://github.com/FodiYes/HardwareMonitor/blob/main/LICENSE)
[![Build](https://img.shields.io/badge/build-passing-brightgreen)]()

## Features

- **Beautiful glassmorphic design** with smooth gradient rings and glow effects
- Real-time monitoring of **CPU**, **GPU**, and **RAM** usage
- **Pin to desktop** mode with full click-through (perfect as an always-visible overlay)
- System tray integration with context menu (Unpin / Close)
- Adjustable global opacity (when not pinned)
- Smooth animated transitions using lerp interpolation
- Supports NVIDIA (via NVML), AMD (via ADL), and fallback universal GPU monitoring (Performance Counter)
- Lightweight and efficient — built with Dear ImGui + DirectX 11
- No background window chrome — fully transparent and borderless

## Screenshots

> (Add actual screenshots here once available)

- Normal mode (top-most with controls)  
- Pinned mode (click-through, controls hidden in tray)

## Requirements

- Windows 10 or Windows 11
- DirectX 11 compatible GPU
- For best GPU detection:
  - NVIDIA: NVML library (usually installed with drivers)
  - AMD: ADL library (usually installed with drivers)

## How to Build

### Prerequisites

- Visual Studio 2022 (or newer) with C++ desktop development workload
- Dear ImGui (included as submodule or already bundled in common setups)

### Steps

1. Clone the repository:
   ```bash
   git clone https://github.com/FodiYes/HardwareMonitor.git
   cd HardwareMonitor

2. Open the `.sln` file in Visual Studio.

3. Build the solution in **Release** or **Debug** mode (x64 recommended).

4. Run the generated executable.

> The app uses `segoeui.ttf` and `segoeuib.ttf` from `C:\Windows\Fonts\`. These fonts are present on all modern Windows installations.

## Usage

- Launch the executable.
- The widget appears in the top-left corner (you can drag it by clicking anywhere except the control area).
- Click the **O** button to **pin** it to the desktop → becomes click-through and moves to the bottom layer.
- When pinned, right-click the tray icon to **Unpin** or **Close**.
- When not pinned, use the **X** button to close or the opacity slider at the bottom.

## Known Issues / Limitations

- Initial window size is small — resize or move as needed.
- GPU temperature is only shown for NVIDIA cards (can be extended).
- No configuration persistence yet (position, opacity, etc.).

## Contributing

Contributions are welcome! Feel free to:

- Open issues for bugs or feature requests
- Submit pull requests with improvements (better GPU detection, settings save, etc.)

Please follow the existing code style and keep the project lightweight.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Author

**FodiYes**  
GitHub: [@FodiYes](https://github.com/FodiYes)

Enjoy the widget! 🚀
```
