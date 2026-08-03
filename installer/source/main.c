#include <switch.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define CONTENTS_DIR "sdmc:/atmosphere/contents/00FF000053484102"
#define EXEFS_PATH CONTENTS_DIR "/exefs.nsp"
#define BOOT_FLAG_PATH CONTENTS_DIR "/boot2.flag"
#define APP_DIR "sdmc:/switch/switch-ha"
#define CONFIG_PATH APP_DIR "/config.ini"
#define TITLES_PATH APP_DIR "/titles.txt"

extern const unsigned char switch_ha_native_nsp_start[];
extern const unsigned char switch_ha_native_nsp_end[];
extern const unsigned char switch_ha_titles_txt_start[];
extern const unsigned char switch_ha_titles_txt_end[];

static bool ensure_dir(const char *path) {
    if (mkdir(path, 0777) == 0) return true;
    return errno == EEXIST;
}

static bool write_file(const char *path, const void *data, size_t size) {
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    const size_t written = fwrite(data, 1, size, file);
    const int close_result = fclose(file);
    return written == size && close_result == 0;
}

static bool create_config(void) {
    static const char config[] =
        "# Switch HA Native MQTT configuration\n"
        "# Complete these values before restarting the console.\n"
        "mqtt_host=\n"
        "mqtt_port=1883\n"
        "mqtt_username=\n"
        "mqtt_password=\n";
    return write_file(CONFIG_PATH, config, sizeof(config) - 1);
}

int main(int argc, char *argv[]) {
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    fsdevMountSdmc();

    printf("Switch HA Native\nMQTT telemetry installer\n\nA: instalar o actualizar\n+: salir\n");
    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 down = padGetButtonsDown(&pad);
        if (down & HidNpadButton_Plus) break;
        if (!(down & HidNpadButton_A)) {
            consoleUpdate(NULL);
            continue;
        }
        printf("\nInstalando...\n");
        const bool dirs = ensure_dir("sdmc:/atmosphere") && ensure_dir("sdmc:/atmosphere/contents") &&
                          ensure_dir(CONTENTS_DIR) && ensure_dir("sdmc:/switch") && ensure_dir(APP_DIR);
        const bool sysmodule = dirs && write_file(EXEFS_PATH, switch_ha_native_nsp_start,
            (size_t)(switch_ha_native_nsp_end - switch_ha_native_nsp_start));
        const bool boot_flag = sysmodule && write_file(BOOT_FLAG_PATH, "", 0);
        const bool titles = boot_flag && write_file(TITLES_PATH, switch_ha_titles_txt_start,
            (size_t)(switch_ha_titles_txt_end - switch_ha_titles_txt_start));
        bool config = titles;
        bool config_created = false;
        if (config) {
            FILE *existing = fopen(CONFIG_PATH, "rb");
            if (existing) { fclose(existing); printf("config.ini existente: conservado.\n"); }
            else { config = create_config(); config_created = config; }
        }
        if (config && config_created) printf("\nPlantilla creada en switch/switch-ha/config.ini. Completa MQTT antes de reiniciar.\n");
        else if (config) printf("\nListo. Reinicia completamente la consola para activar el sysmodule.\n");
        else printf("\nError al instalar. Verifica la SD y vuelve a intentarlo.\n");
        printf("\nPulsa + para salir.\n");
        while (appletMainLoop()) { padUpdate(&pad); if (padGetButtonsDown(&pad) & HidNpadButton_Plus) goto done; consoleUpdate(NULL); }
    }
done:
    fsdevUnmountDevice("sdmc");
    consoleExit(NULL);
    return 0;
}
