#include "kernel/config.h"
#include "fs.h"
#include "libc/util.h"
#include "kernel/memory.h"

#define CONFIG_DIR_CORE "core"
#define CONFIG_DIR_PATH "core/config"
#define CONFIG_FILE "shutdown"

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
    int dummy_row = 0;
    fs_cd("/", &dummy_row);
    if (!fs_exists(CONFIG_DIR_CORE)) return 1;
    fs_cd(CONFIG_DIR_CORE, &dummy_row);
    if (!fs_exists("config")) return 1;
    fs_cd("config", &dummy_row);
    if (!fs_exists(CONFIG_FILE)) return 1;

    uint32_t size = fs_get_size(CONFIG_FILE);
    if (size == 0) return 1;

    uint32_t sectors = (size / 512) + 1;
    uint32_t buffer_size = sectors * 512;
    uint8_t* buffer = (uint8_t*)malloc(buffer_size);
    if (!buffer) return 1;

    int result = 1;
    memset(buffer, 0, buffer_size);
    if (fs_load_to_memory(CONFIG_FILE, buffer)) {
        buffer[size] = '\0';
        if (strstr((char*)buffer, "shutdown=False") != NULL) {
            result = 0;
        }
    }

    free(buffer);
    fs_cd("/", &dummy_row);
    return result;
}

void config_set_shutdown_clean(int clean) {
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

    fs_cd("/", &dummy_row);
}
