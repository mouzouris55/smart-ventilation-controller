# Smart Ventilation Controller

An ESP32-based fan controller that automatically regulates fan speed based on humidity, built to solve condensation problems in a caravan — with full local control, no cloud dependency, and no lag even on weak WiFi.

## Why humidity, not temperature

A single 12V PC fan can't meaningfully cool a space — but it can prevent the condensation and mold that build up when humidity rises against cold surfaces (windows, walls). That's the actual failure mode this project protects against, so humidity is the control signal, not temperature.

## The problem that shaped the design

The first version of this ran as a Home Assistant automation over WiFi. Placed ~20m from the router, every adjustment had a noticeable delay and the fan would jump abruptly to 100% instead of ramping smoothly. The fix was to move all control logic onto the ESP32 itself (edge computing) — Home Assistant is now just a monitoring/remote-control window, never in the real-time decision path.

## Tested behavior

Verified by breathing directly onto the DHT11 to spike humidity in a controlled test (video in `/docs`): humidity rises to ~95% → fan jumps to full speed → sensor dries out over the next ~30-60s → fan steps back down through the tiers (60% → 25% → off) as humidity falls, re-evaluating every 10 seconds. Matches the designed behavior exactly.

## Hardware

| Component | Details |
|---|---|
| Microcontroller | ESP32 DevKit |
| Sensor | DHT11 (temperature/humidity) |
| Manual input | KY-040 rotary encoder |
| Actuator | 12V PC fan, 4-pin (PWM + tachometer) |
| Display | 0.91" I2C OLED (SSD1306, 128×32) |
| Power | 19.5V laptop charger → buck converter → 12V for the fan |

## Wiring

| Signal | ESP32 GPIO |
|---|---|
| DHT11 Data | GPIO4 |
| Encoder CLK | GPIO18 |
| Encoder DT | GPIO19 |
| Encoder SW | GPIO17 |
| OLED SDA | GPIO21 |
| OLED SCK/SCL | GPIO27 |
| Fan PWM | GPIO22 |
| Fan Tachometer | GPIO23 (needs 10kΩ pull-up to 3.3V) |

Buck converter, fan, and ESP32 share one common ground rail.

## How it works

- **Auto mode (default):** fan speed follows humidity — 0% below 55%, ramping up to 100% above 75%, re-checked every 10 seconds.
- **Manual mode:** the rotary encoder sets fan speed directly.
- **Fail-safe:** if WiFi drops, the system automatically forces Auto mode, so the space stays protected even with nobody around.
- **OLED:** always shows live temperature, humidity, current mode, fan %, and real RPM — fully readable and operable with zero network connection.

Full config: [`vent-controller.yaml`](./vent-controller.yaml).

## Media

*(add your photos/video here)*

- `docs/buck-converter.jpg`
- `docs/oled-display.jpg`
- `docs/ha-dashboard.jpg`
- `docs/humidity-test-demo.mp4` — 1-minute test video (breath test on DHT11)

## Next iteration

Rewriting this in plain C++ — hardware and control logic only, no Docker, no Home Assistant. The ESP32 will run standalone and be controlled directly from its own interface.

## Acknowledgments

Built and wired entirely by hand — sensors, power stage, and all the hardware debugging (USB drivers, permissions, wiring). Used Claude (Anthropic) as a coding assistant for the ESPHome YAML and for working through some Linux/Docker setup issues along the way.
