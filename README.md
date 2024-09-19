# WaveWM

**WaveWM** is a tiny window manager for X11 systems. WaveWM provides intuitive keybindings for managing windows and comes with the ability to seamlessly switch between tiling and floating window modes.
![wavewm_screenshot](https://github.com/user-attachments/assets/94e61381-6795-48c3-a9e9-4ab3e17e9dd8)

## Keybindings

| Key Combo           | Action                                    |
| ------------------- | ----------------------------------------- |
| `Alt + LCtrl`       | Kill the focused window                   |
| `Alt + LShift`      | Swap between currently open windows       |
| `Alt + Button1`     | Move the focused window                   |
| `Alt + Button1`     | Tiling: Snap window to the closest screen edge for tiling |
| `Alt + Button2`     | Toggle maximize or center the window      |
| `Alt + Button3`     | Resize the window                         |

**Tiling Feature:**
- When a window is moved close to the screen's edge (using `Alt + Button1`), it will automatically tile to that edge, similar to traditional tiling window managers.
- Tiled windows adjust dynamically to maximize the use of available screen space.
![tilling_screenshot](https://github.com/user-attachments/assets/1ac40157-ecf1-48a1-b5bc-0d31c961e110)

## Features

- **Minimal and Efficient:** Designed to be lightweight and fast.
- **Tiling & Floating Windows:** Automatically tile windows to the edges of the screen or float them freely.
- **Intuitive Keybindings:** Includes simple and customizable key combinations for window management.
- **Customizable Window Manager:** Easily add or change keybindings and functionality.
- **Multi-Window Management:** Quickly swap between open windows and resize or move them with ease.

## Future Features
- Adding background image support.
- More keybindings for advanced window management.
- Improved tiling algorithms.
- Search bar for launching program from.
- Keybindings configuration via a file (instead of hardcoding keybindings).
- Multi-monitor support.



### Dependencies
Make sure the following dependencies are installed on your system:
- X11 libraries (`x11`)
- `libglog` (Google logging library)

Install the required dependencies on a Debian-based system with:
```bash
sudo apt-get install libx11-dev libglog-dev libpng-dev
```

### Resources:
- ![How X Window Managers Work and How to Write One](https://jichu4n.com/posts/how-x-window-managers-work-and-how-to-write-one-part-i/)
