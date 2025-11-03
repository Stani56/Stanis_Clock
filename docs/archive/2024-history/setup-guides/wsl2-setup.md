# ESP32 Serial Port Configuration for Ubuntu under WSL2

This guide provides detailed instructions for setting up serial port access to program and monitor ESP32 devices from Ubuntu running under WSL2.

## Overview

WSL2 doesn't have direct access to Windows serial ports (COM ports). To use ESP32 with ESP-IDF under WSL2, you need to set up USB/serial port passthrough using one of these methods:
1. **usbipd-win** (Recommended) - Native USB passthrough
2. **socat** - Network serial bridge (alternative)

## Method 1: USB Passthrough with usbipd-win (Recommended)

### Prerequisites on Windows

1. **Install usbipd-win**
   - Download from: https://github.com/dorssel/usbipd-win/releases
   - Run the `.msi` installer as Administrator
   - Restart your computer after installation

2. **Install USB drivers for ESP32**
   - Most ESP32 boards use CP2102 or CH340 USB-to-Serial chips
   - CP2102: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
   - CH340: http://www.wch-ic.com/downloads/CH341SER_ZIP.html

### Prerequisites in WSL2 Ubuntu

1. **Update WSL2 kernel** (if needed)
   ```bash
   # In Windows PowerShell (as Administrator)
   wsl --update
   wsl --shutdown
   ```

2. **Install USBIP tools and USB serial drivers in WSL2**
   ```bash
   # In WSL2 Ubuntu
   sudo apt update
   sudo apt install linux-tools-generic hwdata
   sudo update-alternatives --install /usr/local/bin/usbip usbip /usr/lib/linux-tools/*-generic/usbip 20
   
   # Install USB serial drivers
   sudo apt install linux-modules-extra-$(uname -r)
   ```

### Connecting ESP32 to WSL2

1. **List USB devices** (Windows PowerShell as Administrator)
   ```powershell
   usbipd wsl list
   ```
   
   Look for your ESP32 device. It typically shows as:
   - "Silicon Labs CP210x USB to UART Bridge"
   - "USB-SERIAL CH340"
   
   Note the BUSID (e.g., "2-4")

2. **Attach USB device to WSL2**
   ```powershell
   # Replace 2-4 with your device's BUSID
   usbipd wsl attach --busid 2-4
   
   # Or attach to a specific WSL distribution
   usbipd wsl attach --busid 2-4 --distribution Ubuntu
   ```

3. **Verify device in WSL2**
   ```bash
   # In WSL2 Ubuntu
   lsusb
   # Should show your USB device
   
   ls -la /dev/ttyUSB*
   # Should show /dev/ttyUSB0 or similar
   
   # Check device permissions
   ls -la /dev/ttyUSB0
   # Output: crw-rw---- 1 root dialout 188, 0 Jul 4 10:30 /dev/ttyUSB0
   ```

4. **Add user to dialout group** (one-time setup)
   ```bash
   sudo usermod -a -G dialout $USER
   # Log out and back in, or run:
   newgrp dialout
   
   # Verify group membership
   groups
   # Should include 'dialout'
   ```

### Using with ESP-IDF

1. **Flash ESP32**
   ```bash
   cd /home/tanihp/esp_projects/wordclock
   idf.py -p /dev/ttyUSB0 flash
   ```

2. **Monitor serial output**
   ```bash
   idf.py -p /dev/ttyUSB0 monitor
   # Exit with Ctrl+]
   ```

3. **Flash and monitor in one command**
   ```bash
   idf.py -p /dev/ttyUSB0 flash monitor
   ```

### Automating USB Attachment

Create a Windows batch file to automatically attach ESP32:

**esp32-attach.bat**
```batch
@echo off
echo Attaching ESP32 to WSL2...
usbipd wsl attach --busid 2-4
echo ESP32 attached successfully!
pause
```

### Troubleshooting USB Passthrough

1. **Device not showing in WSL2**
   ```bash
   # Check kernel modules
   lsmod | grep usbserial
   
   # If missing, load modules
   sudo modprobe usbserial
   sudo modprobe cp210x  # For CP2102
   sudo modprobe ch341   # For CH340
   ```

2. **Permission denied errors**
   ```bash
   # Fix permissions temporarily
   sudo chmod 666 /dev/ttyUSB0
   
   # Or create udev rule (permanent)
   echo 'SUBSYSTEM=="tty", GROUP="dialout", MODE="0660"' | sudo tee /etc/udev/rules.d/50-usb-serial.rules
   sudo udevadm control --reload-rules
   ```

3. **Device disconnects frequently**
   ```powershell
   # In Windows PowerShell, detach and reattach
   usbipd wsl detach --busid 2-4
   usbipd wsl attach --busid 2-4
   ```

## Method 2: Serial over Network with socat (Alternative)

If USB passthrough doesn't work reliably, use network serial bridging:

### Windows Setup

1. **Install com0com** (virtual COM port pair)
   - Download: https://sourceforge.net/projects/com0com/
   - Install and create a port pair (e.g., COM10 ↔ COM11)

2. **Install socat for Windows**
   - Download from: https://github.com/tech128/socat-windows
   - Extract to `C:\tools\socat\`

3. **Configure ESP32 to use virtual port**
   - Connect ESP32 to real port (e.g., COM3)
   - Use hub4com to bridge: COM3 ↔ COM10

4. **Run socat server on Windows**
   ```cmd
   C:\tools\socat\socat.exe TCP-LISTEN:54321,reuseaddr,fork FILE:COM11,b115200,raw
   ```

### WSL2 Setup

1. **Install socat in WSL2**
   ```bash
   sudo apt install socat
   ```

2. **Create virtual serial port**
   ```bash
   # Get Windows host IP
   export HOST_IP=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}')
   
   # Create virtual port linked to Windows
   sudo socat pty,link=/dev/ttyUSB0,raw,echo=0 tcp:$HOST_IP:54321
   ```

3. **Fix permissions**
   ```bash
   sudo chmod 666 /dev/ttyUSB0
   ```

## ESP-IDF Configuration

### Set default serial port

1. **Project-specific configuration**
   ```bash
   cd /home/tanihp/esp_projects/wordclock
   idf.py menuconfig
   # Navigate to: Serial flasher config → Default serial port
   # Set to: /dev/ttyUSB0
   idf.py save-defconfig
   ```

2. **Environment variable** (applies to all projects)
   ```bash
   # Add to ~/.bashrc
   echo 'export ESPPORT=/dev/ttyUSB0' >> ~/.bashrc
   source ~/.bashrc
   ```

3. **Alias for convenience**
   ```bash
   # Add to ~/.bashrc
   echo 'alias esp32="idf.py -p /dev/ttyUSB0"' >> ~/.bashrc
   source ~/.bashrc
   
   # Usage:
   esp32 flash
   esp32 monitor
   ```

## Best Practices

1. **Always detach before unplugging**
   ```powershell
   usbipd wsl detach --busid 2-4
   ```

2. **Check device before operations**
   ```bash
   # Create a check script
   cat > ~/check-esp32.sh << 'EOF'
   #!/bin/bash
   if [ -e /dev/ttyUSB0 ]; then
       echo "✅ ESP32 connected at /dev/ttyUSB0"
       ls -la /dev/ttyUSB0
   else
       echo "❌ ESP32 not found!"
       echo "Run in Windows: usbipd wsl attach --busid 2-4"
   fi
   EOF
   chmod +x ~/check-esp32.sh
   ```

3. **Monitor connection status**
   ```bash
   # Watch for device changes
   watch -n 1 'ls -la /dev/ttyUSB*'
   ```

## Common Issues and Solutions

### Issue 1: "Cannot open /dev/ttyUSB0: Permission denied"
```bash
# Solution:
sudo usermod -a -G dialout $USER
# Then logout and login again
```

### Issue 2: "Serial port /dev/ttyUSB0 not found"
```powershell
# In Windows PowerShell:
usbipd wsl list
usbipd wsl attach --busid [YOUR-BUSID]
```

### Issue 3: "A fatal error occurred: Failed to connect to ESP32"
```bash
# Check if another process is using the port
sudo lsof /dev/ttyUSB0

# Kill the process if needed
sudo kill -9 [PID]
```

### Issue 4: Slow serial communication
```bash
# Increase buffer sizes
echo 'export ESP_IDF_UART_BUFFER_SIZE=2048' >> ~/.bashrc
source ~/.bashrc
```

## Quick Reference Card

```bash
# Windows PowerShell (Admin)
usbipd wsl list                          # List USB devices
usbipd wsl attach --busid 2-4           # Attach ESP32
usbipd wsl detach --busid 2-4           # Detach ESP32

# WSL2 Ubuntu
ls /dev/ttyUSB*                          # Check serial ports
groups                                   # Verify dialout membership
idf.py -p /dev/ttyUSB0 flash monitor    # Flash and monitor
```

## Using Serial Device in Visual Studio Code

### Prerequisites for VS Code

1. **Install VS Code in Windows** (not in WSL2)
   - Download from: https://code.visualstudio.com/
   - Install with "Add to PATH" option

2. **Install WSL extension**
   - Open VS Code
   - Install "WSL" extension by Microsoft
   - Restart VS Code

3. **Install ESP-IDF extension**
   - In VS Code, install "ESP-IDF" extension by Espressif
   - Configure extension to use WSL2 environment

### Opening Project in WSL2

1. **Open WSL2 terminal and navigate to project**
   ```bash
   cd /home/tanihp/esp_projects/wordclock
   code .
   ```
   
   This opens VS Code in Windows connected to WSL2 filesystem.

2. **Verify WSL connection**
   - Bottom-left corner should show: "WSL: Ubuntu"
   - Terminal in VS Code should be WSL2 bash

### Configuring ESP-IDF Extension for Serial Port

1. **Set up ESP-IDF paths** (Ctrl+Shift+P)
   ```
   ESP-IDF: Configure ESP-IDF Extension
   - IDF Path: /home/tanihp/esp/esp-idf
   - Tools Path: /home/tanihp/.espressif
   ```

2. **Configure serial port** 
   
   Create or edit `.vscode/settings.json` in your project:
   ```json
   {
     "idf.port": "/dev/ttyUSB0",
     "idf.baudRate": "115200",
     "idf.pythonBinPath": "/home/tanihp/.espressif/python_env/idf5.3_py3.10_env/bin/python",
     "idf.flashType": "UART",
     "idf.monitorBaudRate": "115200",
     "terminal.integrated.defaultProfile.linux": "bash",
     "terminal.integrated.profiles.linux": {
       "bash": {
         "path": "/bin/bash"
       }
     }
   }
   ```

3. **Create tasks for common operations**
   
   Create `.vscode/tasks.json`:
   ```json
   {
     "version": "2.0.0",
     "tasks": [
       {
         "label": "Build",
         "type": "shell",
         "command": "idf.py build",
         "group": {
           "kind": "build",
           "isDefault": true
         },
         "problemMatcher": []
       },
       {
         "label": "Flash",
         "type": "shell",
         "command": "idf.py -p /dev/ttyUSB0 flash",
         "group": "build",
         "dependsOn": "Build",
         "problemMatcher": []
       },
       {
         "label": "Monitor",
         "type": "shell",
         "command": "idf.py -p /dev/ttyUSB0 monitor",
         "group": "build",
         "problemMatcher": []
       },
       {
         "label": "Flash & Monitor",
         "type": "shell",
         "command": "idf.py -p /dev/ttyUSB0 flash monitor",
         "group": "build",
         "dependsOn": "Build",
         "problemMatcher": []
       },
       {
         "label": "Clean",
         "type": "shell",
         "command": "idf.py fullclean",
         "group": "build",
         "problemMatcher": []
       }
     ]
   }
   ```

### Using ESP-IDF VS Code Extension Features

1. **Status Bar Functions** (bottom of VS Code)
   - **Build**: Click build icon or `Ctrl+E B`
   - **Flash**: Click flash icon or `Ctrl+E F`
   - **Monitor**: Click monitor icon or `Ctrl+E M`
   - **Port Selection**: Click port name to change

2. **Command Palette** (Ctrl+Shift+P)
   - `ESP-IDF: Build your project`
   - `ESP-IDF: Flash (UART) device`
   - `ESP-IDF: Monitor device`
   - `ESP-IDF: Select port to use`
   - `ESP-IDF: Device configuration`
   - `ESP-IDF: SDK Configuration editor (menuconfig)`

3. **Keyboard Shortcuts**
   ```
   Ctrl+E B    - Build project
   Ctrl+E F    - Flash device
   Ctrl+E M    - Monitor device
   Ctrl+E D    - Build, Flash & Monitor
   Ctrl+E C    - Clean project
   Ctrl+E G    - GUI menuconfig
   ```

### Serial Monitor in VS Code

1. **Using integrated terminal**
   ```bash
   # In VS Code terminal (should be WSL2 bash)
   idf.py -p /dev/ttyUSB0 monitor
   ```
   
   **Monitor controls:**
   - `Ctrl+]` - Exit monitor
   - `Ctrl+T Ctrl+R` - Reset ESP32
   - `Ctrl+T Ctrl+F` - Build and flash
   - `Ctrl+T Ctrl+A` - Pause/Resume output

2. **Using Serial Monitor extension** (alternative)
   - Install "Serial Monitor" extension by Microsoft
   - Configure port: `/dev/ttyUSB0`
   - Set baud rate: `115200`
   - Click "Start Monitoring"

### Troubleshooting VS Code Serial Issues

1. **Extension can't find serial port**
   ```bash
   # In VS Code terminal, verify port exists
   ls -la /dev/ttyUSB0
   
   # If missing, attach in Windows PowerShell
   usbipd wsl attach --busid 2-4
   ```

2. **Permission denied in VS Code**
   ```bash
   # Run in VS Code terminal
   sudo usermod -a -G dialout $USER
   # Restart VS Code completely
   ```

3. **Monitor shows garbled text**
   - Check baud rate in `.vscode/settings.json`
   - Verify ESP32 serial output configuration
   - Default should be 115200

4. **Flash fails from VS Code**
   ```bash
   # Check if port is busy
   sudo lsof /dev/ttyUSB0
   
   # Kill any blocking process
   sudo kill -9 [PID]
   ```

### Best Practices for VS Code Development

1. **Auto-attach script for VS Code**
   
   Create `attach-esp32.sh` in project root:
   ```bash
   #!/bin/bash
   echo "Checking ESP32 connection..."
   if [ ! -e /dev/ttyUSB0 ]; then
       echo "ESP32 not attached. Please run in Windows PowerShell (Admin):"
       echo "  usbipd wsl attach --busid 2-4"
       exit 1
   fi
   echo "✅ ESP32 connected at /dev/ttyUSB0"
   ls -la /dev/ttyUSB0
   ```

2. **Pre-build check task**
   
   Add to `.vscode/tasks.json`:
   ```json
   {
     "label": "Check ESP32",
     "type": "shell",
     "command": "./attach-esp32.sh",
     "group": "none",
     "presentation": {
       "reveal": "always",
       "panel": "new"
     }
   }
   ```

3. **Launch configuration for debugging**
   
   Create `.vscode/launch.json`:
   ```json
   {
     "version": "0.2.0",
     "configurations": [
       {
         "name": "ESP32 OpenOCD",
         "type": "cppdbg",
         "request": "launch",
         "program": "${workspaceFolder}/build/wordclock.elf",
         "miDebuggerPath": "/home/tanihp/.espressif/tools/xtensa-esp32-elf/esp-12.2.0_20230208/xtensa-esp32-elf/bin/xtensa-esp32-elf-gdb",
         "setupCommands": [
           {
             "text": "target remote :3333"
           },
           {
             "text": "monitor reset halt"
           },
           {
             "text": "thb app_main"
           }
         ],
         "externalConsole": false,
         "cwd": "${workspaceFolder}",
         "environment": [],
         "MIMode": "gdb"
       }
     ]
   }
   ```

### VS Code Workspace Recommendations

Create `.vscode/extensions.json` to recommend useful extensions:
```json
{
  "recommendations": [
    "ms-vscode-remote.remote-wsl",
    "espressif.esp-idf-extension",
    "ms-vscode.cpptools",
    "ms-vscode.cmake-tools",
    "ms-vscode.serial-monitor",
    "marus25.cortex-debug"
  ]
}
```

### Quick VS Code Workflow

1. **Open project**
   ```bash
   cd /home/tanihp/esp_projects/wordclock
   code .
   ```

2. **Attach ESP32** (Windows PowerShell Admin)
   ```powershell
   usbipd wsl attach --busid 2-4
   ```

3. **Build, Flash, and Monitor** (in VS Code)
   - Press `Ctrl+E D` for all-in-one operation
   - Or use status bar buttons
   - Or run tasks: `Ctrl+Shift+B` → Select task

4. **Exit monitor**
   - Press `Ctrl+]` in terminal

## Verification Steps

1. **Test serial connection**
   ```bash
   # Simple serial test
   screen /dev/ttyUSB0 115200
   # Exit with Ctrl+A, K, Y
   
   # Or using minicom
   sudo apt install minicom
   minicom -D /dev/ttyUSB0 -b 115200
   # Exit with Ctrl+A, X
   ```

2. **Test with ESP-IDF**
   ```bash
   cd /home/tanihp/esp_projects/wordclock
   idf.py -p /dev/ttyUSB0 flash
   # Should see: "Connecting........_____......."
   # Followed by: "Chip is ESP32-D0WD"
   ```

## Conclusion

With this setup, you can seamlessly develop ESP32 projects in WSL2 Ubuntu with full serial port access. The usbipd-win method is recommended for its reliability and performance. Remember to:

- Always attach the device after connecting it physically
- Add yourself to the dialout group
- Detach before disconnecting the USB cable
- Use the full device path (/dev/ttyUSB0) in ESP-IDF commands

For the Word Clock project, you can now run:
```bash
cd /home/tanihp/esp_projects/wordclock
idf.py -p /dev/ttyUSB0 flash monitor
```

And enjoy full serial communication with your ESP32!