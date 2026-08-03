#include <switch.h>
#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CONTENTS_DIR "sdmc:/atmosphere/contents/00FF000053484102"
#define EXEFS_PATH CONTENTS_DIR "/exefs.nsp"
#define FLAGS_DIR CONTENTS_DIR "/flags"
#define BOOT_FLAG_PATH FLAGS_DIR "/boot2.flag"
#define LEGACY_BOOT_FLAG_PATH CONTENTS_DIR "/boot2.flag"
#define APP_DIR "sdmc:/switch/switch-ha-native"
#define CONFIG_PATH APP_DIR "/config.ini"
#define TITLES_PATH APP_DIR "/titles.txt"
#define LEGACY_APP_DIR "sdmc:/switch/switch-ha"
#define LEGACY_CONFIG_PATH LEGACY_APP_DIR "/config.ini"
#define LEGACY_TITLES_PATH LEGACY_APP_DIR "/titles.txt"

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

static bool file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

static bool copy_file(const char *source_path, const char *destination_path) {
    FILE *source = fopen(source_path, "rb");
    if (!source) return false;
    FILE *destination = fopen(destination_path, "wb");
    if (!destination) { fclose(source); return false; }
    unsigned char buffer[4096];
    bool ok = true;
    size_t read;
    while ((read = fread(buffer, 1, sizeof(buffer), source)) != 0) {
        if (fwrite(buffer, 1, read, destination) != read) { ok = false; break; }
    }
    if (ferror(source)) ok = false;
    if (fclose(source) != 0) ok = false;
    if (fclose(destination) != 0) ok = false;
    return ok;
}

static bool remove_tree(const char *path) {
    DIR *directory = opendir(path);
    if (!directory) return remove(path) == 0 || errno == ENOENT;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char child[512];
        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        struct stat status;
        if (stat(child, &status) == 0 && S_ISDIR(status.st_mode)) {
            if (!remove_tree(child)) { closedir(directory); return false; }
        } else if (remove(child) != 0 && errno != ENOENT) {
            closedir(directory);
            return false;
        }
    }
    closedir(directory);
    return rmdir(path) == 0 || errno == ENOENT;
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

static bool install_or_preserve(const char *destination_path, const char *legacy_path, const void *data, size_t size) {
    if (file_exists(destination_path)) return true;
    if (file_exists(legacy_path)) return copy_file(legacy_path, destination_path);
    return write_file(destination_path, data, size);
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
                          ensure_dir(CONTENTS_DIR) && ensure_dir(FLAGS_DIR) && ensure_dir("sdmc:/switch") && ensure_dir(APP_DIR);
        const bool sysmodule = dirs && write_file(EXEFS_PATH, switch_ha_native_nsp_start,
            (size_t)(switch_ha_native_nsp_end - switch_ha_native_nsp_start));
        const bool boot_flag = sysmodule && write_file(BOOT_FLAG_PATH, "", 0);
        if (boot_flag) remove(LEGACY_BOOT_FLAG_PATH);
        const bool titles = boot_flag && install_or_preserve(TITLES_PATH, LEGACY_TITLES_PATH,
            switch_ha_titles_txt_start, (size_t)(switch_ha_titles_txt_end - switch_ha_titles_txt_start));
        bool config = titles;
        bool config_created = false;
        if (config) {
            FILE *existing = fopen(CONFIG_PATH, "rb");
            if (existing) { fclose(existing); printf("config.ini existente: conservado.\n"); }
            else if (file_exists(LEGACY_CONFIG_PATH)) { config = copy_file(LEGACY_CONFIG_PATH, CONFIG_PATH); }
            else { config = create_config(); config_created = config; }
        }
        const bool legacy_removed = config && remove_tree(LEGACY_APP_DIR);
        if (config && config_created && legacy_removed) printf("\nPlantilla creada en switch/switch-ha-native/config.ini. Completa MQTT antes de reiniciar.\n");
        else if (config && legacy_removed) printf("\nListo. Reinicia completamente la consola para activar el sysmodule.\n");
        else printf("\nError al instalar. Verifica la SD y vuelve a intentarlo.\n");
        printf("\nPulsa + para salir.\n");
        while (appletMainLoop()) { padUpdate(&pad); if (padGetButtonsDown(&pad) & HidNpadButton_Plus) goto done; consoleUpdate(NULL); }
    }
done:
    fsdevUnmountDevice("sdmc");
    consoleExit(NULL);
    return 0;
}
