# System Manager (`sys-mgr`)

**System Manager** is the core background service responsible for managing
all system-level operations in the **Cronos** embedded Linux platform.
It acts as the **central control layer**, handling hardware access, task
scheduling, and inter-process coordination through a unified D-Bus interface.

---

## 🚀 Overview

`sys-mgr` is implemented in **C**, designed for **embedded Linux** systems
built with **Yocto** and running on **STM32MP157D-DK1**.
It operates entirely in the background, exposing **D-Bus services** for
system control and monitoring while internally orchestrating hardware access
and asynchronous task execution.

The **Terminal UI** communicates with `sys-mgr` **only through D-Bus**.
It does not directly interact with the hardware or any system APIs — all
system operations, from Wi-Fi scanning to sensor updates or PWM control,
are performed by `sys-mgr`.

---

## 🧩 Key Features

- **📡 Network & Modem Control**  
  Integrates with **NetworkManager** and **ModemManager** via their client
  libraries (`libnm`, `libmm-glib`).  
  Provides unified APIs for managing Wi-Fi, Gigabit+Megabit Ethernet (W5500),
  and cellular connections.

- **🧠 Workqueue-based Execution Model**  
  Uses a **custom lightweight workqueue** system for asynchronous task
  scheduling.  
  Tasks are queued from two primary sources:  
  1. **D-Bus commands** from the UI  
  2. **Hardware monitor loops** (periodic sensor reads or events)  
  Each task runs with assigned priority and duration category
  (`WORK_PRIO_*`, `WORK_DURATION_*`).

- **🌡️ Hardware Integration**  
  Directly manages and monitors peripheral devices:
  - Ambient light sensor (**OPT3001**, I²C)
  - IMU (**MPU6500**, I²C/SPI)
  - Pressure/temperature sensor (**BMP280**, I²C/SPI)
  - Screen brightness via PWM (**PH11**)
  - Dual vibration motors via PWM (**PC7**, **PD13**)

- **🔄 Hardware Monitor Loop**  
  Periodic loop that polls sensors, validates readings, and queues work
  requests to update cached data or trigger system actions.  

---

## 🧱 Architecture

```text
+------------------------------------------------------------------------------+
| OS (Yocto rootfs + Linux kernel 6.6)                                         |
|                                                                              |
|  +---------------------------------------------------+   +----------------+  |
|  |                  System Manager                   |   | Kernel Drivers |  |
|  | +----------------+     +------------------------+ |   +----------------+  |
|  | | Workqueue Core | --> | Async IO / GLib Loop   | |           ↑           |
|  | +----------------+     +------------------------+ |           |           |
|  |         ↑                                         |           ↓           |
|  | +----------------+                                |    +---------------+  |
|  | | DBus Interface | <------------------------------->   |  Terminal UI  |  |
|  | +----------------+  (libdbus)                     |    +---------------+  |
|  |         |                                         |                       |
|  | +----------------+                                |                       |
|  | | HW Monitor     |---> Periodic Tasks via WorkQ   |                       |
|  | | (Sensors/PWM)  |                                |                       |
|  | | NM/MM Clients  |                                |                       |
|  | +----------------+                                |                       |
|  +---------------------------------------------------+                       |
|                                                                              |
+------------------------------------------------------------------------------+
```

---

## 🧰 Build Instructions

### Prerequisites
- Yocto-built Linux (tested on **STM32MP157D-DK1**)
- **glib-2.0**
- **libdbus-1**
- **libnm (1.46+)**
- **libmm-glib**
- **I2C/SPI kernel drivers enabled**
- **CMake** or **Makefile** build system

### Build Steps

```bash
git clone https://github.com/asmc-waltz/sys-mgr.git
cd sys-mgr
mkdir build && cd build
cmake .. -G "Unix Makefiles"
# Resolve all required dependencies
make -j$(nproc)
```

---

## ⚙️ Logging & Error Handling

- Uses **Linux-standard log levels** (`LOG_ERROR`, `LOG_INFO`, `LOG_DEBUG`, …)
- Follows **`errno.h`** for all return codes (`-EIO`, `-EINVAL`, `-ENOMEM`, …)

---

## ✅ Supported

- **DBus** communication  
- **NetworkManager** communication  
- **Vibration** control 
- **Brightness/Ambient light sensor** control 
- **MPU sensor** control 

---

## 🔮 Future Extensions

- Add a Systemd service file for the application
  to start as a daemon within systemd
- Implement **power management hooks** for suspend/resume
- Migrate from **Standard C library** to **glib** for
  improved maintainability and type safety
- Integrate **OTA update** handling and diagnostic reporting
- Add **hardware + software watchdog** supervision
- Expand **sensor fusion pipeline** for motion and environment analysis
- Implement hardware communication using a standard
  for increased hardware compatibility.

