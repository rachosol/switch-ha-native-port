# Switch HA Native

Native Stratosphere MQTT telemetry for Nintendo Switch and Home Assistant.

Maintained by [rachosol](https://github.com/rachosol). This is a focused port
based on the ideas and assets of [ErSeraph's Switch Assistant](https://github.com/ErSeraph/switch-assistant);
the upstream MIT license is included in [LICENSE](LICENSE), with attribution in
[NOTICE.md](NOTICE.md).

## What it installs

Launch `switch-ha-native.nro` from the Homebrew Menu. It writes the native
sysmodule to `atmosphere/contents/00FF000053484102/exefs.nsp`, creates the
required `boot2.flag`, installs `switch/switch-ha/titles.txt`, and creates
`switch/switch-ha/config.ini` on first use. Existing configuration is never
overwritten. Restart the console completely after installation.

The installer asks only for the MQTT broker IPv4 address, username, and password.
The sysmodule uses its built-in client ID, normal MQTT port 1883, and Home
Assistant discovery prefix `homeassistant`.

Published telemetry: console/game state, game title and ID, battery level,
voltage, temperature, health, charging state, and heartbeat.

It intentionally contains no streaming, RTSP, overlay, HTTP token, remote
power controls, notifications, audio, brightness, or controller features.

## Build

Requirements: devkitA64, devkitPro/libnx, Docker (optional), and the
Atmosphere-libs submodule.

```sh
git submodule update --init --recursive
make
```

The GitHub-ready executable is written to `dist/switch-ha-native.nro`. To use
the already tested local Atmosphere-libs checkout instead of the submodule:

```sh
make ATMOSPHERE_LIBS=/path/to/Atmosphere-libs
```

## Configuration

The generated `switch/switch-ha/config.ini` is deliberately minimal:

```ini
mqtt_host=192.168.1.10
mqtt_username=homeassistant
mqtt_password=change-me
```

The current native network client accepts an IPv4 address, not a DNS hostname.
Do not commit a real configuration file or credentials.
