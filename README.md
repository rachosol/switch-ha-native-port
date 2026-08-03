# Switch HA Native

## English

Native Stratosphere MQTT telemetry for Nintendo Switch and Home Assistant.

Maintained by [rachosol](https://github.com/rachosol). This is a focused port
based on the ideas and assets of [ErSeraph's Switch Assistant](https://github.com/ErSeraph/switch-assistant).
The upstream MIT license is included in [LICENSE](LICENSE), with attribution in
[NOTICE.md](NOTICE.md).

### What it installs

Launch `switch-ha-native.nro` from the Homebrew Menu. It installs the native
sysmodule at `atmosphere/contents/00FF000053484102/exefs.nsp`, creates the
required `boot2.flag`, and uses `switch/switch-ha-native/` for the NRO,
`titles.txt`, and `config.ini`. Existing configuration and title database are
never overwritten. Restart the console completely after installation.

On first use, the installer creates a blank MQTT template and does not modify
an existing configuration. The sysmodule uses its built-in client ID and the
Home Assistant discovery prefix `homeassistant`.

Published telemetry: console/game state, game title and ID, battery level,
voltage, temperature, health, charging state, and heartbeat.

It intentionally contains no streaming, RTSP, overlay, Home Assistant HTTP
token, remote power controls, notifications, audio, brightness, or controller
features.

### Compatibility

This release was built and validated with **Atmosphère 1.10.1** (Horizon OS
21.1.0). No Atmosphère update is planned for the validated setup at this time.
Use on any other Atmosphère or Horizon OS version is untested and entirely at
the user's own discretion and risk.

### Build

Requirements: devkitA64, devkitPro/libnx, Docker (optional), and the
Atmosphere-libs submodule.

```sh
git submodule update --init --recursive
make
```

The GitHub-ready executable is written to `dist/switch-ha-native.nro`. To use
the tested local Atmosphere-libs checkout instead of the submodule:

```sh
make ATMOSPHERE_LIBS=/path/to/Atmosphere-libs
```

### Switch configuration

The generated `switch/switch-ha-native/config.ini` is deliberately minimal:

```ini
mqtt_host=
mqtt_port=1883
mqtt_username=
mqtt_password=
```

Fill `mqtt_host`, `mqtt_username`, and `mqtt_password` yourself. `mqtt_port`
is configurable and defaults to 1883. The native network client accepts an
IPv4 address, not a DNS hostname. Do not commit real credentials.

### Add a missing game title

`switch/switch-ha-native/titles.txt` is preserved during program updates. To
add a game not included in the database, edit that file on a computer and add
one line using the exact format below:

```text
0100ABCDEF123000;My Game Title
```

Use the game's 16-character hexadecimal Title ID, followed by a semicolon and
the desired display name. Save the file as UTF-8 text. Because the installer
never overwrites an existing `titles.txt`, custom entries survive updates.

### Set up MQTT in Home Assistant

Switch HA Native requires an MQTT broker reachable from the Switch. Home
Assistant recommends its official **Mosquitto broker** app.

1. In Home Assistant, open **Settings → Apps → App store**, search for
   **Mosquitto broker**, install it, and start it.
2. Open the app's **Configuration** tab and add a dedicated Switch login.
   Choose a long, unique password:

   ```yaml
   logins:
     - username: switch_ha
       password: replace-with-a-long-unique-password
   ```

   Save the configuration and restart the Mosquitto broker app. Do not reuse a
   Home Assistant administrator account or commit this password to GitHub.
3. In **Settings → Devices & services**, add the **MQTT** integration if it is
   not already present. The setup wizard can configure the official broker
   automatically; verify that MQTT discovery is enabled and its prefix is
   `homeassistant`.
4. Find the local IPv4 address of Home Assistant or the broker host, then fill
   in the Switch template:

   ```ini
   mqtt_host=192.168.1.10
   mqtt_port=1883
   mqtt_username=switch_ha
   mqtt_password=replace-with-a-long-unique-password
   ```

5. Restart the Switch completely. The Nintendo Switch device should appear
   through MQTT discovery in roughly 30 seconds.

The official [Home Assistant MQTT documentation](https://www.home-assistant.io/integrations/mqtt/)
explains broker installation, integration setup, and additional logins.

---

## Español

Telemetría MQTT nativa de Stratosphere para Nintendo Switch y Home Assistant.

Mantenido por [rachosol](https://github.com/rachosol). Es un port enfocado,
basado en las ideas y los recursos de [Switch Assistant de ErSeraph](https://github.com/ErSeraph/switch-assistant).
La licencia MIT original se incluye en [LICENSE](LICENSE), con atribución en
[NOTICE.md](NOTICE.md).

### Qué instala

Inicia `switch-ha-native.nro` desde Homebrew Menu. Instala el sysmodule nativo
en `atmosphere/contents/00FF000053484102/exefs.nsp`, crea el `boot2.flag`
requerido y usa `switch/switch-ha-native/` para el NRO, `titles.txt` y
`config.ini`. Nunca sobrescribe una configuración ni una base de títulos
existente. Reinicia completamente la consola después de instalar.

En el primer uso, el instalador crea una plantilla MQTT vacía y no modifica una
configuración existente. El sysmodule usa su client ID integrado y el prefijo
de discovery de Home Assistant `homeassistant`.

Telemetría publicada: estado de consola/juego, título e ID del juego, nivel de
batería, voltaje, temperatura, salud, estado de carga y heartbeat.

No incluye deliberadamente streaming, RTSP, overlay, token HTTP de Home
Assistant, controles remotos de energía, notificaciones, audio, brillo ni
funciones de mandos.

### Compatibilidad

Esta versión fue compilada y validada con **Atmosphère 1.10.1** (Horizon OS
21.1.0). Por el momento no se planea actualizar Atmosphère en la instalación
validada. El uso con cualquier otra versión de Atmosphère o de Horizon OS no
ha sido probado y queda completamente a discreción y riesgo de cada usuario.

### Compilación

Requisitos: devkitA64, devkitPro/libnx, Docker (opcional) y el submódulo
Atmosphere-libs.

```sh
git submodule update --init --recursive
make
```

El ejecutable listo para GitHub se genera en `dist/switch-ha-native.nro`. Para
usar la copia local verificada de Atmosphere-libs en vez del submódulo:

```sh
make ATMOSPHERE_LIBS=/ruta/a/Atmosphere-libs
```

### Configuración de Switch

El archivo generado `switch/switch-ha-native/config.ini` se limita a lo necesario:

```ini
mqtt_host=
mqtt_port=1883
mqtt_username=
mqtt_password=
```

Completa tú mismo `mqtt_host`, `mqtt_username` y `mqtt_password`.
`mqtt_port` es configurable y su valor predeterminado es 1883. El cliente de
red nativo acepta una dirección IPv4, no un nombre DNS. No subas credenciales
reales al repositorio.

### Añadir el título de un juego faltante

`switch/switch-ha-native/titles.txt` se conserva durante las actualizaciones
del programa. Para añadir un juego que no está en la base de datos, edita ese
archivo en un computador y agrega una línea con este formato exacto:

```text
0100ABCDEF123000;Título de mi juego
```

Usa el Title ID hexadecimal de 16 caracteres del juego, seguido de punto y
coma y el nombre que deseas mostrar. Guarda el archivo como texto UTF-8. Como
el instalador nunca sobrescribe un `titles.txt` existente, las entradas propias
se conservan tras las actualizaciones.

### Configurar MQTT en Home Assistant

Switch HA Native necesita un broker MQTT accesible desde la Switch. Home
Assistant recomienda su aplicación oficial **Mosquitto broker**.

1. En Home Assistant, abre **Settings → Apps → App store**, busca
   **Mosquitto broker**, instálalo e inícialo.
2. Abre la pestaña **Configuration** de la aplicación y añade un login dedicado
   para la Switch. Elige una contraseña larga y única:

   ```yaml
   logins:
     - username: switch_ha
       password: replace-with-a-long-unique-password
   ```

   Guarda la configuración y reinicia la aplicación Mosquitto broker. No
   reutilices una cuenta administradora de Home Assistant ni subas esta
   contraseña a GitHub.
3. En **Settings → Devices & services**, añade la integración **MQTT** si aún
   no existe. El asistente puede configurar automáticamente el broker oficial;
   verifica que MQTT discovery esté habilitado y que el prefijo sea
   `homeassistant`.
4. Obtén la IPv4 local de Home Assistant o del host del broker y completa la
   plantilla de Switch:

   ```ini
   mqtt_host=192.168.1.10
   mqtt_port=1883
   mqtt_username=switch_ha
   mqtt_password=replace-with-a-long-unique-password
   ```

5. Reinicia completamente la Switch. El dispositivo Nintendo Switch debería
   aparecer mediante MQTT discovery en unos 30 segundos.

La [documentación oficial de MQTT de Home Assistant](https://www.home-assistant.io/integrations/mqtt/)
explica la instalación del broker, la integración y los logins adicionales.
