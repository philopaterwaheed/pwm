# pwm window_manager (philo's window manager)
X11 window manager for linux built from scratch in C++. It supports basic window management features such as tiling, floating windows, multi-monitor setups, workspaces, and more. The window manager is designed to be lightweight, efficient, and customizable.
inspired by dwm ; we can say it's a love letter to dwm  . 
![wakatime](https://wakatime.com/badge/user/0f89acde-73e3-4e5a-b142-f273fd933144/project/c29b98b2-64ec-44a4-a4e5-dff25fc505e7.svg)


## Features

- **Tiling Layout**: Windows are arranged in a tiling layout by default.
- **Floating Windows**: Support for floating windows, which can move freely across monitors.
- **Multi-Monitor Support**: Handles multiple monitors with independent workspaces and window layouts for each screen.
- **Dynamic Workspaces**: Switch between multiple workspaces, with sticky windows that remain visible across workspaces.
- **Mouse and Keyboard Interaction**: Resize, move, and manage windows with both keyboard shortcuts and mouse actions.
- **Bar with Workspaces**: A customizable status bar showing active workspaces, which updates based on the `xsetroot` command.
- **Window Snapping**: Windows snap to the edges of the screen when moved close.
- **Fullscreen Windows**: Support for windows requesting fullscreen mode, such as browsers in fullscreen video mode.
- **Center Master Layout**: A layout option where the main window is centered, with other windows tiled around it.
- **Sticky Clients**: Windows can be set as "sticky" so they remain visible across all workspaces.
- **Resizable Borders**: Configurable window borders, useful for visual feedback and resizing.
- **Monitor Focus and Workspace Movement**: Easily switch focus between monitors based on mouse movement and keyboard shortcuts.

## Dependencies

This window manager is built using the X11 library. Ensure that you have the following dependencies installed:

- **Xlib**: For X11 window system interaction.
  ```bash
  sudo apt-get install libx11-dev
  ```
- **Xinerama**: For multi-monitor support.
  ```bash
  sudo apt-get install libxinerama-dev
  ```
- **Fontconfig**: For font rendering in the status bar.
  ```bash
  sudo apt-get install libfontconfig1-dev
  ```

## Installation

1. **Clone the repository**:
   ```bash
   git https://github.com/philopaterwaheed/pwm.git
   cd pwm
   ```

2. **Compile the window manager**:
   ```bash
   sudo make clean install
   ```

3. **Run the window manager**:
   You can test the window manager by launching it from a different X session:
   ```bash
   startx ./pwm -- :1
   ```
   or add it to your `.xinitrc` script

## Usage

### Keybindings
you can customize the Keybindings in the `config.h` file 
but those are the default 
| Keybinding                    | Action                                 |
| ----------------------------- | -------------------------------------- |
| `Mod + t`                     | Launch terminal (default: `st`)        |
| `Mod + ;`                     | Launch terminal (default: `dmenu`) you also can use mine if you want [launchio](https://github.com/philopaterwaheed/launchio.git)                       |
| `Mod +  Arrow Keys`           | Move floating window                   |
| `Mod + Shift + Arrow Keys`    | Resize floating window                 |
| `Mod + Left Mouse Button`     | Drag window                            |
| `Mod + Right Mouse Button`    | Resize window                          |
| `Mod + 1-5`                   | Switch to workspace                    |
| `Mod + Shift + 1-5`           | Move window to workspace               |
| `Mod + q`                     | Close focused window                   |
| `Mod + Return`                | Make focuse window fullscreen          |
| `Mod + s`                     | Make focuse window sticky              |
| `Mod + b`                     | Toggle bar                             |
| `Mod + l`                     | Increase master width                  |
| `Mod + h`                     | Decrease master width                  |
| `Mod + j`                     | Change focus to next window            |
| `Mod + k`                     | Change focus to previous window        |
| `Mod + Shift + j`             | Swap with next window                  |
| `Mod + Shift + k`             | Swap with previous window              |
| `Mod + Shift + Space`         | Toggle floating of focused window      |
| `Mod + Shift + f`             | Set as master                          |
| `Mod + Shift + l`             | Increase stacked window hight          |
| `Mod + Shift + h`             | Decrease stacked window hight          |
| `Mod + Alt + Space`           | Switch to tiling layout                |
| `Mod + Alt + m`               | Switch to fullscreen layout            |
| `Mod + Alt + c`               | Switch to center master layout         |
| `Mod + Alt + g`               | Switch to grid layout                  |
| `Mod + ,`                     | Focus next monitor                     |
| `Mod + .`                     | Focus previous monitor                 |

### Mouse Actions

- **Move Window**: Hold `Mod` + Left Click to drag a floating window.
- **Resize Window**: Hold `Mod` + Right Click to resize a floating window.
- **Snapping**: Floating windows snap to screen edges when moved close to them.

### Workspace Management

- **Workspaces**: Use `Mod + 1-5` to switch between workspaces. Each monitor has its own set of workspaces.
- **Sticky Windows**: Mark a window as sticky to keep it visible across all workspaces of the monitor.
- **Moving Windows Between Workspaces**: Use `Mod + Shift + [1-5]` to move a window to a different workspace.
- you can increase the number if you want by changing the config 
### Bar Configuration

The status bar displays the active workspaces and the current status. You can update the bar by setting the root window name using the `xsetroot` command. For example:
```bash
xsetroot -name "Hello, World!"
```


### Floating and Tiling Windows

- **Tiling**: Windows are tiled by default in the main workspace. The active window takes up the master area, with other windows stacked.
- **Floating**: Floating windows are always placed on top of tiled windows and can be moved freely between monitors.
- **Fullscreen**: Windows that request fullscreen (like a browser in video fullscreen mode) are handled and displayed accordingly.

### Layouts

The window manager supports the following layouts:

- **Tiling**: Default layout where windows are tiled.
- **Floating**: Windows can be freely moved and resized.
- **Center Master**: The master window is centered, with other windows tiled around it.
- **Grid**: Windows are arranged in a grid layout.

### Window Resizing and Moving

- **Mouse Resizing**: Resize windows with `Mod + Right Click`.
- **Mouse Moving**: Move floating windows with `Mod + Left Click`.
- **Keyboard Resizing**: Resize floating windows using keyboard shortcuts.

## Advanced Configuration

### Fonts

Fonts in the bar are configured using the Fontconfig library. Ensure that you have appropriate fonts installed (e.g., `awesome-font` for icons).

### Customizing the Bar

The status bar is divided into three sections:
- Left: Workspace names `note: ` you can set the names of hte workspaces in the config file but please use one icone becuase the wm doesn't detect the lendth of the name right 
- Right: Custom status (set with `xsetroot`)


You can modify the bar by editing the rendering logic in the source code.


## Contributions

Feel free to contribute by submitting pull requests, reporting issues, or suggesting features.

## License

This window manager is released under the MIT License.

---

# screen shots
![2024-09-23_06-13](https://github.com/user-attachments/assets/0010defd-ab08-4cd8-a8f7-86ba099cab38)
![2024-09-23_06-27](https://github.com/user-attachments/assets/8c93975a-fc55-480f-9dca-c623a04b3d54)
![2024-09-23_06-29](https://github.com/user-attachments/assets/a0be1844-4d0f-4902-823d-aea5e781af53)
![2024-09-23_06-31](https://github.com/user-attachments/assets/9a10e87a-3362-4b32-908a-c93297595b50)
