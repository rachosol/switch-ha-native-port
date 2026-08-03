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

On first use, the installer creates a blank MQTT template and never modifies
an existing configuration. The sysmodule uses its built-in client ID and Home
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
mqtt_host=
mqtt_port=1883
mqtt_username=
mqtt_password=
```

Fill `mqtt_host`, `mqtt_username`, and `mqtt_password` yourself. `mqtt_port`
is configurable and defaults to 1883. The native network client accepts an
IPv4 address, not a DNS hostname. Do not commit real credentials.

## Configurar MQTT en Home Assistant

Switch HA Native necesita un broker MQTT accesible desde la Switch. La opción
recomendada por Home Assistant es su aplicación oficial **Mosquitto broker**.

1. En Home Assistant, abre **Settings → Apps → App store**, busca
   **Mosquitto broker**, instálalo e inícialo.
2. Abre la pestaña **Configuration** de la aplicación y añade un login dedicado
   para la Switch. Elige una contraseña única y robusta:

   ```yaml
   logins:
     - username: switch_ha
       password: replace-with-a-long-unique-password
   ```

   Guarda la configuración y reinicia la aplicación Mosquitto broker. No
   reutilices una cuenta de administrador de Home Assistant ni publiques esta
   contraseña en GitHub.
3. En **Settings → Devices & services**, añade la integración **MQTT** si aún
   no existe. El asistente puede configurar automáticamente el broker oficial;
   verifica que MQTT discovery esté habilitado y que el prefijo sea
   `homeassistant`.
4. Obtén la IPv4 local de Home Assistant o del host del broker y completa la
   plantilla de la Switch:

   ```ini
   mqtt_host=192.168.1.10
   mqtt_port=1883
   mqtt_username=switch_ha
   mqtt_password=replace-with-a-long-unique-password
   ```

5. Reinicia completamente la Switch. En aproximadamente 30 segundos aparecerá
   el dispositivo Nintendo Switch mediante MQTT discovery.

La guía oficial de Home Assistant para MQTT explica la instalación del broker,
la integración y la gestión de logins adicionales:
[MQTT - Home Assistant](https://www.home-assistant.io/integrations/mqtt/).
