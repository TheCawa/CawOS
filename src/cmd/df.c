#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "fs.h"
#include "kernel/memory.h"

extern uint16_t cawfat[MAX_CLUSTERS];
extern file_t fs[MAX_FILES];

static void df_check(int* row, int used_clusters) {
    uint8_t* visited = (uint8_t*)malloc(MAX_CLUSTERS);
    if (!visited) {
        print_line_scroll("Error: Not enough memory.", 0, row, 0x0C);
        return;
    }
    memset(visited, 0, MAX_CLUSTERS);
    int referenced = 0;
    int bad_chains = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs[i].exists || fs[i].is_dir) continue;

        uint32_t expected = (fs[i].size_bytes + 511) / 512;
        uint32_t actual = 0;
        uint16_t cur = fs[i].first_cluster;

        while (cur != CLUSTER_EOF && cur < MAX_CLUSTERS) {
            if (!visited[cur]) {
                visited[cur] = 1;
                referenced++;
            }
            actual++;
            if (actual > expected) break;
            cur = cawfat[cur];
        }

        if (actual != expected) bad_chains++;
    }
    int orphans = used_clusters - referenced;
    if (orphans < 0) orphans = 0;
    char line[64];
    char nb[16];
    memset(line, 0, 64);
    strcpy(line, "  Referenced by files: ");
    itoa(referenced, nb); strcat(line, nb);
    print_line_scroll(line, 0, row, 0x0F);
    memset(line, 0, 64);
    strcpy(line, "  Orphaned clusters: ");
    itoa(orphans, nb); strcat(line, nb);
    print_line_scroll(line, 0, row, orphans > 0 ? 0x0C : 0x0A);
    memset(line, 0, 64);
    strcpy(line, "  Files with bad chain length: ");
    itoa(bad_chains, nb); strcat(line, nb);
    print_line_scroll(line, 0, row, bad_chains > 0 ? 0x0C : 0x0A);
    free(visited);
}

void cmd_df(char* args, int* row) {
    int check = 0;
    if (args && (strcasecmp(args, "--check") == 0 || strcasecmp(args, "-c") == 0)) {
        check = 1;
    }
    int total_clusters = MAX_CLUSTERS - 1;
    int free_clusters = 0;
    for (int i = 1; i < MAX_CLUSTERS; i++) {
        if (cawfat[i] == CLUSTER_EMPTY) free_clusters++;
    }
    int used_clusters = total_clusters - free_clusters;
    int file_slots = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists) file_slots++;
    }
    int total_kb = total_clusters / 2;
    int used_kb  = used_clusters / 2;
    int free_kb  = free_clusters / 2;
    int percent  = (used_clusters * 100) / total_clusters;
    char line[80];
    char nb[16];
    print_line_scroll("CawFS disk usage:", 0, row, 0x0B);
    memset(line, 0, 80);
    strcpy(line, "  Total: ");
    itoa(total_kb, nb); strcat(line, nb); strcat(line, " KB (");
    itoa(total_clusters, nb); strcat(line, nb); strcat(line, " clusters)");
    print_line_scroll(line, 0, row, 0x0F);
    memset(line, 0, 80);
    strcpy(line, "  Used:  ");
    itoa(used_kb, nb); strcat(line, nb); strcat(line, " KB (");
    itoa(used_clusters, nb); strcat(line, nb); strcat(line, " clusters)");
    print_line_scroll(line, 0, row, 0x0F);
    memset(line, 0, 80);
    strcpy(line, "  Free:  ");
    itoa(free_kb, nb); strcat(line, nb); strcat(line, " KB (");
    itoa(free_clusters, nb); strcat(line, nb); strcat(line, " clusters)");
    print_line_scroll(line, 0, row, 0x0F);
    memset(line, 0, 80);
    strcpy(line, "  Files: ");
    itoa(file_slots, nb); strcat(line, nb); strcat(line, "/");
    itoa(MAX_FILES, nb); strcat(line, nb); strcat(line, " slots");
    print_line_scroll(line, 0, row, 0x0F);
    print_line_scroll("  Cluster size: 512 bytes", 0, row, 0x07);
    char bar[64];
    int p = 0;
    int filled = (used_clusters * 40) / total_clusters;
    bar[p++] = ' '; bar[p++] = ' '; bar[p++] = '[';
    for (int i = 0; i < 40; i++) bar[p++] = (i < filled) ? '#' : '.';
    bar[p++] = ']';
    bar[p++] = ' ';
    itoa(percent, nb);
    for (int i = 0; nb[i] != '\0'; i++) bar[p++] = nb[i];
    bar[p++] = '%';
    bar[p] = '\0';
    print_line_scroll(bar, 0, row, 0x0E);
    if (check) {
        df_check(row, used_clusters);
    }
}
REGISTER_COMMAND("df", cmd_df, 1);