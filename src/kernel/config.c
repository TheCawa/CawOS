#include "kernel/config.h"
#include "fs.h"
#include "libc/util.h"
#include "kernel/memory.h"

#define CONFIG_DIR_CORE "core"
#define CONFIG_DIR_PATH "core/config"
#define CONFIG_FILE "shutdown"
#define CONFIG_FILE_IDLE "idle"

static void config_ensure_dir() {
    int dummy_row = 0;
    fs_cd("/", &dummy_row);
    if (!fs_exists(CONFIG_DIR_CORE)) {
        fs_mkdir(CONFIG_DIR_CORE, &dummy_row);
    }
    fs_cd(CONFIG_DIR_CORE, &dummy_row);
    if (!fs_exists("config")) {
        fs_mkdir("config", &dummy_row);
    }
    fs_cd("/", &dummy_row);
}

void config_init() {
    config_ensure_dir();
    config_set_shutdown_clean(0);
}

int config_was_shutdown_clean() {
    char saved_dir[32];
    strcpy(saved_dir, current_dir);

    int result = 1;
    int dummy_row = 0;
    fs_cd("/", &dummy_row);
    if (fs_exists(CONFIG_DIR_CORE)) {
        fs_cd(CONFIG_DIR_CORE, &dummy_row);
        if (fs_exists("config")) {
            fs_cd("config", &dummy_row);
            if (fs_exists(CONFIG_FILE)) {
                uint32_t size = fs_get_size(CONFIG_FILE);
                if (size > 0) {
                    uint32_t sectors = (size / 512) + 1;
                    uint32_t buffer_size = sectors * 512;
                    uint8_t* buffer = (uint8_t*)malloc(buffer_size + 1);
                    if (buffer) {
                        memset(buffer, 0, buffer_size + 1);
                        if (fs_load_to_memory(CONFIG_FILE, buffer)) {
                            buffer[size] = '\0';
                            if (strstr((char*)buffer, "shutdown=False") != NULL) {
                                result = 0;
                            }
                        }
                        free(buffer);
                    }
                }
            }
        }
    }

    fs_cd_abs(saved_dir);
    return result;
}

void config_set_shutdown_clean(int clean) {
    char saved_dir[32];
    strcpy(saved_dir, current_dir);

    config_ensure_dir();

    int dummy_row = 0;
    fs_cd("/", &dummy_row);
    fs_cd(CONFIG_DIR_CORE, &dummy_row);
    fs_cd("config", &dummy_row);
    if (!fs_exists(CONFIG_FILE)) {
        fs_create(CONFIG_FILE, &dummy_row);
    }

    char content[64];
    snprintf(content, sizeof(content), "shutdown=%s\n", clean ? "True" : "False");
    fs_write(CONFIG_FILE, (uint8_t*)content, strlen(content));

    fs_cd_abs(saved_dir);
}

void config_set_idle(int enabled, int minutes) {
    char saved_dir[32];
    strcpy(saved_dir, current_dir);

    config_ensure_dir();
    int dummy_row = 0;
    fs_cd("/", &dummy_row);
    fs_cd(CONFIG_DIR_CORE, &dummy_row);
    fs_cd("config", &dummy_row);
    if (!fs_exists(CONFIG_FILE_IDLE)) {
        fs_create(CONFIG_FILE_IDLE, &dummy_row);
    }

    char content[32];
    memset(content, 0, 32);
    if (enabled) {
        snprintf(content, sizeof(content), "idle=%d\n", minutes);
    } else {
        strcpy(content, "idle=off\n");
    }
    fs_write(CONFIG_FILE_IDLE, (uint8_t*)content, strlen(content));

    fs_cd_abs(saved_dir);
}

void config_get_idle(int* enabled, int* minutes) {
    *enabled = 1;
    *minutes = 1;

    char saved_dir[32];
    strcpy(saved_dir, current_dir);

    int dummy_row = 0;
    fs_cd("/", &dummy_row);
    if (!fs_exists(CONFIG_DIR_CORE)) { fs_cd_abs(saved_dir); return; }
    fs_cd(CONFIG_DIR_CORE, &dummy_row);
    if (!fs_exists("config")) { fs_cd_abs(saved_dir); return; }
    fs_cd("config", &dummy_row);
    if (!fs_exists(CONFIG_FILE_IDLE)) { fs_cd_abs(saved_dir); return; }

    uint32_t size = fs_get_size(CONFIG_FILE_IDLE);
    if (size == 0) { fs_cd_abs(saved_dir); return; }

    uint32_t sectors = (size / 512) + 1;
    uint8_t* buffer = (uint8_t*)malloc(sectors * 512 + 1);
    if (!buffer) { fs_cd_abs(saved_dir); return; }
    memset(buffer, 0, sectors * 512 + 1);

    if (fs_load_to_memory(CONFIG_FILE_IDLE, buffer)) {
        buffer[size] = '\0';
        char* p = strstr((char*)buffer, "idle=");
        if (p != NULL) {
            p += 5;
            if (strncasecmp(p, "off", 3) == 0) {
                *enabled = 0;
            } else {
                int m = atoi(p);
                if (m > 0) {
                    *enabled = 1;
                    *minutes = m;
                } else {
                    *enabled = 0;
                }
            }
        }
    }

    free(buffer);
    fs_cd_abs(saved_dir);
}