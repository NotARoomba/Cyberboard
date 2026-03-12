<h1 align="center">
  <br>
  <a href="https://notaroomba.dev"><img src="https://raw.githubusercontent.com/NotARoomba/cyberboard/main/assets/logo.png" alt="Cyberboard V2" width="200"></a>
  <br>
  Cyberboard V2
  <br>
</h1>

<h4 align="center">
A Raspberry Pi Pico-sized STM32 development board with Bluetooth, battery support, IMU and a barometer!
</h4>

<div align="center">

![KiCad](https://img.shields.io/badge/kicad-%2300578F.svg?style=for-the-badge&logo=kicad&logoColor=white)
![STM32](https://img.shields.io/badge/STM32-03234B?style=for-the-badge&logo=stmicroelectronics&logoColor=white)

</div>

<p align="center">
  <a href="#key-features">Key Features</a> •
  <a href="#pcb">PCB</a> •
  <a href="#credits">Credits</a> •
  <a href="#license">License</a>
</p>

<img src="assets/banner.png" alt="Cyberboard Banner" width="800"/>

> **⚠️ Looking for V1?** The first version of the Cyberboard is available in the [v1 branch](https://github.com/notaroomba/cyberboard/tree/v1), but be warned: it doesn't work. V2 is a complete redesign with proper functionality.

<img src="assets/final.png" alt="Cyberboard Banner" width="800"/>

## Key Features

- **STM32WB55CG** microcontroller with Bluetooth support
- **Raspberry Pi Pico form factor** for compatibility
- **IMU and Barometer** - ICM-42688 (6-axis) and BMP580 pressure sensor for environmental/motion sensing
- **Battery charging** with BQ24072RGTR IC and solder pad for 1S LiPo
- **Buck-Boost converter** (TPS631000) for efficient power management
- **Multiple Status LEDs** for visual feedback
- **USB-C** connectivity
- **Ceramic Bluetooth antenna** with impedance matching
- **SWD test points** on the bottom for easy programming
- **4-layer PCB** design with optimized RF layout and routing
- **Panelized design** for cost-effective manufacturing

## PCB

Designed in KiCad with attention to RF design, power management, and sensor integration. The board features a 4-layer stackup (SIG/GND/PWR/SIG) with optimized routing for the Bluetooth antenna and sensitive analog signals.

### Schematic

<img src="assets/schematic.png" alt="Schematic" width="800"/>

### PCB Layers

The 4-layer stackup (SIG/GND/PWR/SIG) provides optimal signal integrity and power distribution:

**Layer 1 (Signal):**

<img src="assets/layer_1.png" alt="PCB Layer 1" width="800"/>

**Layer 2 (Ground):**

<img src="assets/layer_2.png" alt="PCB Layer 2" width="800"/>

**Layer 3 (Power):**

<img src="assets/layer_3.png" alt="PCB Layer 3" width="800"/>

**Layer 4 (Signal):**

<img src="assets/layer_4.png" alt="PCB Layer 4" width="800"/>

**Front:**

<img src="assets/pcb_front.png" alt="PCB Front" width="800"/>

**Back:**

<img src="assets/pcb_back.png" alt="PCB Back" width="800"/>

### JLCPCB Order

<img src="assets/jlcpcb.png" alt="PCB Back" width="800"/>

## Credits

This project uses:

- [KiCad](https://www.kicad.org/)
- [Blender](https://www.blender.org/) for 3D renders
- [Hack Club Blueprint](https://blueprint.hackclub.com/projects/491)

## You may also like...

- [CyberCard](https://github.com/NotARoomba/CyberCard) – A Cyberpunk themed NFC hacker card
- [Ember](https://github.com/NotARoomba/ember) – USB-C PD Powered Hotplate
- [Trace](https://github.com/NotARoomba/trace) – An ruler for engineers
- [Athena](https://github.com/NotARoomba/athena) – A flight controlelr with a triple MCU architecture

## License

MIT

---

> [notaroomba.dev](https://notaroomba.dev) &nbsp;&middot;&nbsp;
> GitHub [@NotARoomba](https://github.com/NotARoomba) &nbsp;&middot;&nbsp;
