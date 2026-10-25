#include "commands.h"
#include "drivers/screen.h"
#include "libc/cpuid.h"
#include "libc/format.h"
#include "libc/string.h"
#include "libc/timer.h"

typedef struct {
    unsigned int bit;
    const char* name;
} feat_t;

static unsigned long long rdtsc64(void) {
    unsigned int lo;
    unsigned int hi;

    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));

    return ((unsigned long long)hi << 32) | (unsigned long long)lo;
}

static void trim_right(char* s) {
    int len = strlen(s);

    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

static int append_feature(char* line, int size, const char* name, int count) {
    int len = strnlen(line, size);
    int name_len = strlen(name);

    if (count > 0) {
        if (len + 1 + name_len >= size) {
            return count;
        }

        line[len] = ' ';
        line[len + 1] = '\0';
        len++;
    } else {
        if (len + name_len >= size) {
            return count;
        }
    }

    strcat(line, name);
    return count + 1;
}

static void cpuid_help(int* row) {
    print_line_scroll("Usage: cpuid [OPTION]", 0, row, 0x0B);
    print_line_scroll("", 0, row, 0x0F);
    print_line_scroll("Options:", 0, row, 0x0E);
    print_line_scroll("  (no args)      Show full CPU information", 0, row, 0x0F);
    print_line_scroll("  --brand        Show processor brand string", 0, row, 0x0F);
    print_line_scroll("  --vendor       Show CPU vendor ID", 0, row, 0x0F);
    print_line_scroll("  --family       Show Family/Model/Stepping", 0, row, 0x0F);
    print_line_scroll("  --features     Show supported CPU features", 0, row, 0x0F);
    print_line_scroll("  --cache        Show cache information", 0, row, 0x0F);
    print_line_scroll("  --hypervisor   Detect virtualization", 0, row, 0x0F);
    print_line_scroll("  --topology     Show cores/threads info", 0, row, 0x0F);
    print_line_scroll("  --speed        Measure CPU frequency", 0, row, 0x0F);
    print_line_scroll("  --raw <leaf>   Raw CPUID leaf dump (hex)", 0, row, 0x0F);
    print_line_scroll("  --help         Show this help message", 0, row, 0x0F);
}

static void cpuid_show_brand(int* row) {
    char brand[49];
    char line[64];

    cpuid_get_brand(brand);

    snprintf(line, sizeof(line), "Brand: %s", brand);
    print_line_scroll(line, 0, row, 0x0F);
}

static void cpuid_show_vendor(int* row) {
    char vendor[13];
    char line[32];

    cpuid_get_vendor(vendor);

    snprintf(line, sizeof(line), "Vendor: %s", vendor);
    print_line_scroll(line, 0, row, 0x0F);
}

static void cpuid_show_family(int* row) {
    if (cpuid_max_basic() < 1) {
        print_line_scroll("Family info not available", 0, row, 0x0C);
        return;
    }

    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;

    cpuid_raw(1, 0, &eax, &ebx, &ecx, &edx);

    unsigned int stepping = eax & 0xFu;
    unsigned int base_model = (eax >> 4) & 0xFu;
    unsigned int base_family = (eax >> 8) & 0xFu;
    unsigned int ext_model = (eax >> 16) & 0xFu;
    unsigned int ext_family = (eax >> 20) & 0xFFu;
    unsigned int family = base_family;

    if (base_family == 0xFu) {
        family = base_family + ext_family;
    }

    unsigned int model = base_model;

    if (base_family == 0x6u || base_family == 0xFu) {
        model = (ext_model << 4) | base_model;
    }

    char line[64];

    snprintf(line, sizeof(line), "Family: %u", family);
    print_line_scroll(line, 0, row, 0x0F);

    snprintf(line, sizeof(line), "Model: %u", model);
    print_line_scroll(line, 0, row, 0x0F);

    snprintf(line, sizeof(line), "Stepping: %u", stepping);
    print_line_scroll(line, 0, row, 0x0F);
}

static void cpuid_show_features(int* row) {
    if (cpuid_max_basic() < 1) {
        print_line_scroll("Features not available", 0, row, 0x0C);
        return;
    }

    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;

    cpuid_raw(1, 0, &eax, &ebx, &ecx, &edx);

    print_line_scroll("Features:", 0, row, 0x0E);

    static const feat_t edx_features[] = {
        {0, "FPU"}, {4, "TSC"}, {5, "MSR"}, {6, "PAE"},
        {8, "CMPXCHG8B"}, {11, "SEP"}, {15, "CMOV"},
        {19, "CLFSH"}, {23, "MMX"}, {24, "FXSR"},
        {25, "SSE"}, {26, "SSE2"}, {28, "HT"}
    };

    static const feat_t ecx_features[] = {
        {0, "SSE3"}, {1, "PCLMULQDQ"}, {9, "SSSE3"},
        {12, "FMA"}, {19, "SSE4.1"}, {20, "SSE4.2"},
        {25, "AES"}, {26, "XSAVE"}, {28, "AVX"},
        {29, "F16C"}, {30, "RDRAND"}
    };

    char line[80];
    int count;

    safe_strcpy(line, "  ", sizeof(line));
    count = 0;

    for (unsigned int i = 0; i < sizeof(edx_features) / sizeof(edx_features[0]); i++) {
        if (edx & (1u << edx_features[i].bit)) {
            count = append_feature(line, sizeof(line), edx_features[i].name, count);
        }
    }

    if (count > 0) {
        print_line_scroll(line, 0, row, 0x0A);
    }

    safe_strcpy(line, "  ", sizeof(line));
    count = 0;

    for (unsigned int i = 0; i < sizeof(ecx_features) / sizeof(ecx_features[0]); i++) {
        if (ecx & (1u << ecx_features[i].bit)) {
            count = append_feature(line, sizeof(line), ecx_features[i].name, count);
        }
    }

    if (count > 0) {
        print_line_scroll(line, 0, row, 0x0A);
    }

    unsigned int max_ext = cpuid_max_extended();

    if (max_ext >= 0x80000001u) {
        cpuid_raw(0x80000001u, 0, &eax, &ebx, &ecx, &edx);

        safe_strcpy(line, "  ", sizeof(line));
        count = 0;

        if (edx & (1u << 29)) {
            count = append_feature(line, sizeof(line), "x86-64", count);
        }

        if (edx & (1u << 27)) {
            count = append_feature(line, sizeof(line), "RDTSCP", count);
        }

        if (edx & (1u << 20)) {
            count = append_feature(line, sizeof(line), "NX", count);
        }

        if (ecx & (1u << 5)) {
            count = append_feature(line, sizeof(line), "LZCNT", count);
        }

        if (ecx & (1u << 8)) {
            count = append_feature(line, sizeof(line), "3DNow!Pref", count);
        }

        if (ecx & (1u << 31)) {
            count = append_feature(line, sizeof(line), "3DNow!", count);
        }

        if (count > 0) {
            print_line_scroll(line, 0, row, 0x0A);
        }
    }
}

static void cpuid_show_cache(int* row) {
    print_line_scroll("Cache:", 0, row, 0x0E);
    unsigned int max_leaf = cpuid_max_basic();
    if (max_leaf < 4) {
        print_line_scroll("  Cache info not available", 0, row, 0x0F);
        return;
    }
    int found = 0;
    for (unsigned int i = 0; i < 10; i++) {

        unsigned int eax;
        unsigned int ebx;
        unsigned int ecx;
        unsigned int edx;

        cpuid_raw(4, i, &eax, &ebx, &ecx, &edx);
        unsigned int cache_type = eax & 0x1Fu;
        if (cache_type == 0) {
            break;
        }
        found = 1;

        unsigned int cache_level = (eax >> 5) & 0x7u;
        unsigned int ways = ((ebx >> 22) & 0x3FFu) + 1;
        unsigned int partitions = ((ebx >> 12) & 0x3FFu) + 1;
        unsigned int line_size = (ebx & 0xFFFu) + 1;
        unsigned int sets = ecx + 1;
        unsigned long long total_size =
            (unsigned long long)ways *
            (unsigned long long)partitions *
            (unsigned long long)line_size *
            (unsigned long long)sets;
        unsigned int size_kb = (unsigned int)(total_size / 1024ULL);
        const char* type_str;

        switch (cache_type) {
            case 1:
                type_str = "Data";
                break;

            case 2:
                type_str = "Instruction";
                break;

            case 3:
                type_str = "Unified";
                break;

            default:
                type_str = "Unknown";
                break;
        }
        char line[64];
        snprintf(
            line,
            sizeof(line),
            "  L%u %s Cache: %u KB",
            cache_level,
            type_str,
            size_kb
        );
        print_line_scroll(line, 0, row, 0x0F);
    }
    if (!found) {
        print_line_scroll("  No cache entries found", 0, row, 0x0F);
    }
}
static void cpuid_show_hypervisor(int* row) {
    if (!cpuid_is_hypervisor()) {
        print_line_scroll("Hypervisor: None (bare metal)", 0, row, 0x0F);
        return;
    }
    print_line_scroll("Hypervisor: Detected", 0, row, 0x0E);

    char sig[13];
    cpuid_hypervisor_signature(sig);
    const char* hv_name = "Unknown";
    if (strncasecmp(sig, "KVMKVMKVM", 9) == 0) {
        hv_name = "KVM";
    } else if (strncasecmp(sig, "Microsoft Hv", 12) == 0) {
        hv_name = "Microsoft Hyper-V";
    } else if (strncasecmp(sig, "VMwareVMware", 12) == 0) {
        hv_name = "VMware";
    } else if (strncasecmp(sig, "VBoxVBoxVBox", 12) == 0) {
        hv_name = "VirtualBox";
    } else if (strncasecmp(sig, "XenVMMXenVMM", 12) == 0) {
        hv_name = "Xen";
    } else if (strncasecmp(sig, "prl ", 4) == 0) {
        hv_name = "Parallels";
    } else if (strncasecmp(sig, "QEMU", 4) == 0) {
        hv_name = "QEMU";
    }

    char line[64];
    snprintf(line, sizeof(line), "Type: %s", hv_name);
    print_line_scroll(line, 0, row, 0x0F);

    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
    cpuid_raw(0x40000000u, 0, &eax, &ebx, &ecx, &edx);
    snprintf(line, sizeof(line), "Max hypervisor leaf: 0x%x", eax);
    print_line_scroll(line, 0, row, 0x0F);
}

static int parse_leaf(const char* s, unsigned int* out) {
    if (s == 0 || *s == '\0') {
        return 0;
    }
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s == '\0') {
        return 0;
    }
    int base = 10;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    }
    unsigned int value = 0;
    int has_digit = 0;
    while (*s) {
        unsigned int digit;
        if (*s >= '0' && *s <= '9') {
            digit = *s - '0';
        } else if (base == 16 && *s >= 'a' && *s <= 'f') {
            digit = *s - 'a' + 10;
        } else if (base == 16 && *s >= 'A' && *s <= 'F') {
            digit = *s - 'A' + 10;
        } else if (*s == ' ' || *s == '\t') {
            s++;

            while (*s == ' ' || *s == '\t') {
                s++;
            }

            if (*s != '\0') {
                return 0;
            }

            break;
        } else {
            return 0;
        }
        has_digit = 1;
        if (value > (0xFFFFFFFFu - digit) / (unsigned int)base) {
            return 0;
        }
        value = value * (unsigned int)base + digit;
        s++;
    }
    if (!has_digit) {
        return 0;
    }
    *out = value;
    return 1;
}

static void cpuid_show_raw(const char* arg, int* row) {
    unsigned int leaf;
    if (!parse_leaf(arg, &leaf)) {
        print_line_scroll("Usage: cpuid --raw <leaf>", 0, row, 0x0C);
        print_line_scroll("Example: cpuid --raw 1", 0, row, 0x0E);
        print_line_scroll("Example: cpuid --raw 0x80000000", 0, row, 0x0E);
        return;
    }

    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
    cpuid_raw(leaf, 0, &eax, &ebx, &ecx, &edx);
    char line[64];
    snprintf(line, sizeof(line), "CPUID Leaf 0x%x:", leaf);
    print_line_scroll(line, 0, row, 0x0E);
    snprintf(line, sizeof(line), "  EAX: 0x%x", eax);
    print_line_scroll(line, 0, row, 0x0F);
    snprintf(line, sizeof(line), "  EBX: 0x%x", ebx);
    print_line_scroll(line, 0, row, 0x0F);
    snprintf(line, sizeof(line), "  ECX: 0x%x", ecx);
    print_line_scroll(line, 0, row, 0x0F);
    snprintf(line, sizeof(line), "  EDX: 0x%x", edx);
    print_line_scroll(line, 0, row, 0x0F);
}

static void cpuid_show_topology(int* row) {
    print_line_scroll("CPU Topology:", 0, row, 0x0E);
    unsigned int max_leaf = cpuid_max_basic();
    char line[64];
    if (max_leaf >= 0xBu) {
        unsigned int eax;
        unsigned int ebx;
        unsigned int ecx;
        unsigned int edx;
        cpuid_raw(0xBu, 0, &eax, &ebx, &ecx, &edx);
        unsigned int threads_per_core = ebx & 0xFFFFu;
        unsigned int eax1;
        unsigned int ebx1;
        unsigned int ecx1;
        unsigned int edx1;
        cpuid_raw(0xBu, 1, &eax1, &ebx1, &ecx1, &edx1);
        unsigned int total_threads = ebx1 & 0xFFFFu;
        if (threads_per_core != 0 && total_threads != 0) {
            unsigned int cores = total_threads / threads_per_core;
            snprintf(line, sizeof(line), "  Cores: %u", cores);
            print_line_scroll(line, 0, row, 0x0F);
            snprintf(line, sizeof(line), "  Threads per core: %u", threads_per_core);
            print_line_scroll(line, 0, row, 0x0F);
            snprintf(line, sizeof(line), "  Total threads: %u", total_threads);
            print_line_scroll(line, 0, row, 0x0F);
            return;
        }

        print_line_scroll("  Topology info not available", 0, row, 0x0F);
        return;
    }
    if (max_leaf >= 1) {
        unsigned int eax;
        unsigned int ebx;
        unsigned int ecx;
        unsigned int edx;
        cpuid_raw(1, 0, &eax, &ebx, &ecx, &edx);
        unsigned int logical_cpus = (ebx >> 16) & 0xFFu;
        snprintf(line, sizeof(line), "  Logical processors: %u", logical_cpus);
        print_line_scroll(line, 0, row, 0x0F);
        print_line_scroll("  (Extended topology not supported)", 0, row, 0x07);
        return;
    }
    print_line_scroll("  Topology info not available", 0, row, 0x0F);
}

static void cpuid_show_speed(int* row) {
    print_line_scroll("Measuring CPU speed...", 0, row, 0x0E);
    if (cpuid_max_basic() < 1) {
        print_line_scroll("TSC not available", 0, row, 0x0C);
        return;
    }
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
    cpuid_raw(1, 0, &eax, &ebx, &ecx, &edx);
    if (!(edx & (1u << 4))) {
        print_line_scroll("TSC not available", 0, row, 0x0C);
        return;
    }
    unsigned long long t1 = rdtsc64();
    sleep_ms(50);
    unsigned long long t2 = rdtsc64();
    unsigned long long elapsed64 = t2 - t1;
    if (elapsed64 > 0xFFFFFFFFULL) {
        print_line_scroll("CPU too fast to measure!", 0, row, 0x0C);
        return;
    }
    unsigned int elapsed = (unsigned int)elapsed64;
    unsigned int freq_mhz = elapsed / 50000u;
    char line[64];
    snprintf(line, sizeof(line), "CPU Speed: ~%u MHz", freq_mhz);
    print_line_scroll(line, 0, row, 0x0F);
    snprintf(line, sizeof(line), "Cycles in 50ms: %u", elapsed);
    print_line_scroll(line, 0, row, 0x07);
}

static void cpuid_show_full(int* row) {
    print_line_scroll("=== CPU Information ===", 0, row, 0x0B);
    print_line_scroll("", 0, row, 0x0F);
    cpuid_show_vendor(row);
    print_line_scroll("", 0, row, 0x0F);
    cpuid_show_brand(row);
    print_line_scroll("", 0, row, 0x0F);
    cpuid_show_family(row);
    print_line_scroll("", 0, row, 0x0F);
    cpuid_show_topology(row);
    print_line_scroll("", 0, row, 0x0F);
    cpuid_show_features(row);
    print_line_scroll("", 0, row, 0x0F);
    cpuid_show_cache(row);
    print_line_scroll("", 0, row, 0x0F);
    cpuid_show_hypervisor(row);
    print_line_scroll("", 0, row, 0x0F);
    cpuid_show_speed(row);
}

void cmd_cpuid(char* args, int* row) {
    char argbuf[128];
    safe_strcpy(argbuf, args ? args : "", sizeof(argbuf));
    char* p = argbuf;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    trim_right(p);
    if (*p == '\0') {
        cpuid_show_full(row);
        return;
    }
    if (strcasecmp(p, "--help") == 0 || strcasecmp(p, "-h") == 0) {
        cpuid_help(row);
    } else if (strcasecmp(p, "--brand") == 0) {
        cpuid_show_brand(row);
    } else if (strcasecmp(p, "--vendor") == 0) {
        cpuid_show_vendor(row);
    } else if (strcasecmp(p, "--family") == 0) {
        cpuid_show_family(row);
    } else if (strcasecmp(p, "--features") == 0) {
        cpuid_show_features(row);
    } else if (strcasecmp(p, "--cache") == 0) {
        cpuid_show_cache(row);
    } else if (strcasecmp(p, "--hypervisor") == 0) {
        cpuid_show_hypervisor(row);
    } else if (strcasecmp(p, "--topology") == 0) {
        cpuid_show_topology(row);
    } else if (strcasecmp(p, "--speed") == 0) {
        cpuid_show_speed(row);
    } else if (
        strncasecmp(p, "--raw", 5) == 0 &&
        (p[5] == '\0' || p[5] == ' ' || p[5] == '\t')
    ) {
        char* leaf_arg = p + 5;

        while (*leaf_arg == ' ' || *leaf_arg == '\t') {
            leaf_arg++;
        }

        cpuid_show_raw(leaf_arg, row);
    } else {
        char line[96];
        snprintf(line, sizeof(line), "Unknown option: %s", p);
        print_line_scroll(line, 0, row, 0x0C);
        print_line_scroll("Use 'cpuid --help' for available options", 0, row, 0x0E);
    }
}

REGISTER_COMMAND("cpuid", cmd_cpuid, 1);