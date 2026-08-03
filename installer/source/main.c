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

static bool prompt_text(const char *guide, char *out, size_t out_size, bool password) {
    SwkbdConfig keyboard;
    if (R_FAILED(swkbdCreate(&keyboard, 0))) return false;
    swkbdConfigSetGuideText(&keyboard, guide);
    swkbdConfigSetStringLenMax(&keyboard, out_size - 1);
    swkbdConfigSetInitialText(&keyboard, out);
    if (password) swkbdConfigSetPasswordFlag(&keyboard, true);
    const Result rc = swkbdShow(&keyboard, out, out_size);
    swkbdClose(&keyboard);
    return R_SUCCEEDED(rc) && out[0] != '\0';
}

static bool create_config(void) {
    char host[64] = {};
    char username[96] = {};
    char password[128] = {};
    printf("Configuracion MQTT inicial.\n\n");
    if (!prompt_text("Direccion IPv4 del broker MQTT", host, sizeof(host), false) ||
        !prompt_text("Usuario MQTT", username, sizeof(username), false) ||
        !prompt_text("Contrasena MQTT", password, sizeof(password), true)) return false;
    char config[512];
    const int length = snprintf(config, sizeof(config),
        "# Switch HA Native\n# Generado por el instalador; conserva solo MQTT.\n"
        "mqtt_host=%s\nmqtt_username=%s\nmqtt_password=%s\n",
        host, username, password);
    return length > 0 && (size_t)length < sizeof(config) && write_file(CONFIG_PATH, config, (size_t)length);
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
        if (config) {
            FILE *existing = fopen(CONFIG_PATH, "rb");
            if (existing) { fclose(existing); printf("config.ini existente: conservado.\n"); }
            else config = create_config();
        }
        if (config) printf("\nListo. Reinicia completamente la consola para activar el sysmodule.\n");
        else printf("\nError al instalar. Verifica la SD y vuelve a intentarlo.\n");
        printf("\nPulsa + para salir.\n");
        while (appletMainLoop()) { padUpdate(&pad); if (padGetButtonsDown(&pad) & HidNpadButton_Plus) goto done; consoleUpdate(NULL); }
    }
done:
    fsdevUnmountDevice("sdmc");
    consoleExit(NULL);
    return 0;
}
