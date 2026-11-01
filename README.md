# Smart Thermostat

An open-source, high-integrity smart thermostat engineered to give **complete local control to the hardware owner**. This project combines custom physical hardware with a local-first software architecture to deliver predictive, intuitive HVAC management entirely independent of mandatory cloud ecosystems.

---

## 🧭 Core Philosophy

* **True Sovereignty:** The device firmware executes entirely on local hardware. It functions completely without mandatory internet-facing cloud sync, external tracking, or third-party background services.
* **System Integrity:** The entire system—encompassing both the custom physical PCB layer and the underlying firmware stack—is completely transparent, audit-ready, and entirely free of hidden telemetry or subversive data mining.
* **Intuitive Self-Sufficiency:** The on-device user interface is designed to be fully self-contained. End-users are never forced to parse terminal logs or inspect source code to diagnose system states; errors or unusual conditions are instantly translated into clear, actionable remedies right on the screen.

---

## ✨ Key Features

### Intelligent HVAC Automation
* **Context-Aware Comfort:** Adapts temperature setpoints dynamically based on localized weather forecasts.
* **Intelligent Ventilation:** Compares indoor air parameters against external conditions to suggest opening a window when ambient weather aligns with target comfort or when lower indoor air quality is detected.
* **Open-Structure Interlocking:** Automatically detects open doors or windows via ecosystem sensors and dynamically disables the active heating/cooling cycles to prevent energy waste.
* **Balancing & Predictive Maintenance:** Integrates remote temperature sensors to eliminate room-to-room temperature differentials and continuously monitors thermal rise/fall rate degradation to identify clogged air filters or mechanical service requirements.

### Connectivity & Control Layers
* **Local-First Ecosystems:** Built with native MQTT and Home Assistant integration for secure, local automation networking. Future milestones include native **Matter over Wi-Fi**.
* **Direct Administration:** Serves a robust, completely local configuration website for adjustments and provides secure Telnet/SSH management interfaces for localized diagnostic logs, debugging, and system metrics.
* **Hardware Interactivity:** Built around an elegant touchscreen user interface featuring zero mechanical face buttons, protected by a dedicated hardware master-reset circuit for ultimate fail-safe recovery.

---

## 🛠️ Hardware Specification

The physical platform is optimized for low power, clean power delivery, and transparent circuit execution.

* **MCU:** Espressif ESP32 (Dual Core, 240MHz, 4MB Flash) running entirely on a native **ESP-IDF** runtime environment. A dedicated On-Chip SWD interface is built-in for deep GDB hardware debugging and direct firmware loading.
* **Display & Touch:** 3.2" SPI TFT Display Module (ILI9341 driver paired with an XPT2046 touch controller). The UI layers and menu navigation frameworks are constructed using the open-source **LVGL (v8)** graphics library.
* **Power Regulation:** Built-in dual-stage industrial regulation translating standard 24VAC HVAC lines down to a ultra-low ripple (<2%) 5VDC rail via an LM2576HVT-5, regulated cleanly to a 3.3V digital rail via an AMS1117 LDO.
* **On-Board Sensor Arrays:**
  * **Climate:** Aosong AHT20 high-precision I2C Temperature & Humidity module.
  * **Presence:** Hi-Link LD2410 24GHz Microwave Radar Human Presence Sensor for localized wake-on-motion automation.
  * **Ambient:** Integrated Light Dependent Resistor (LDR) divider circuit for reactive, step-less TFT backlight dimming matching room conditions.

---

## 💻 Operating Environment & System Architecture

The core application runs deterministically under **FreeRTOS**. Resource allocation is strictly divided across priority-tiered tasks to keep the interface highly responsive:

| Task Name | Execution / Trigger | Priority tier | Purpose |
| :--- | :--- | :--- | :--- |
| `touch` | Hardware Interrupt (Low Touch Line) | Highest | Captures coordinates, processes UI input, triggers audible interface indicator. |
| `motion` | Hardware Interrupt (High Motion Line) | High | Updates human presence timestamps immediately upon radar detection. |
| `aht` | Deterministic (10-second intervals) | Medium-Low | Samples physical environmental sensor registers. |
| `state-machine` | Continuous (1-second loop pumping) | Lowest | Evaluates HVAC states, ticks safety timers, processes menu routing, maps reactive screen brightness via LDR readings, and sleeps the panel when the room is vacant. |

---

## 🤝 Contributing & Community Standards

We welcome engineering contributions from the open-source community! To maintain high code safety and absolute compliance with the project's vision, all contributors must align with our core repository governance:

1. **GPLv3 Compliance:** This project is proudly licensed under the **GNU General Public License, Version 3**. All downstream modifications, clones, or derivative software distributions must remain open, share identical copyleft freedoms, and must explicitly include complete installation information on consumer products.
2. **AI-Assisted Authorship Policy:** Code developed using AI tools is welcome. However, the human contributor assumes 100% accountability for the code. You must possess the complete technical capability to fully explain and defend every line, architectural choice, and flag during peer review.
3. **Compilation Warning Policy:** Code submissions should target a 100% clean compilation. If a specific compiler warning cannot be avoided due to legacy library quirks, it *must* be explicitly declared and technically justified in your Pull Request text for review by the Maintainers.
4. **Behavioral Code:** All interactions in our community tracking systems or communication channels are governed strictly by our **Contributor Covenant Code of Conduct**.

For details on setting up your compiler environment, partition schemes, or debugging crash logs via `esp-coredump`, please refer to our internal development wikis and the `CONTRIBUTING.md` guidelines.