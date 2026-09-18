#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"

static void do_cpuid(uint32_t eax, uint32_t* out_eax, uint32_t* out_ebx,
                     uint32_t* out_ecx, uint32_t* out_edx) {
    __asm__ volatile("cpuid"
        : "=a"(*out_eax), "=b"(*out_ebx), "=c"(*out_ecx), "=d"(*out_edx)
        : "a"(eax)
    );
}

static void cpuid_get_vendor(char* buf) {
    uint32_t eax, ebx, ecx, edx;
    do_cpuid(0, &eax, &ebx, &ecx, &edx);
    buf[0] = ebx & 0xFF; buf[1] = (ebx >> 8) & 0xFF;
    buf[2] = (ebx >> 16) & 0xFF; buf[3] = (ebx >> 24) & 0xFF;
    buf[4] = edx & 0xFF; buf[5] = (edx >> 8) & 0xFF;
    buf[6] = (edx >> 16) & 0xFF; buf[7] = (edx >> 24) & 0xFF;
    buf[8] = ecx & 0xFF; buf[9] = (ecx >> 8) & 0xFF;
    buf[10] = (ecx >> 16) & 0xFF; buf[11] = (ecx >> 24) & 0xFF;
    buf[12] = '\0';
}

static void cpuid_get_brand(char* buf) {
    uint32_t regs[4];
    uint32_t max_ext;
    do_cpuid(0x80000000, &max_ext, &regs[1], &regs[2], &regs[3]);
    
    if (max_ext < 0x80000004) {
        strcpy(buf, "Unknown");
        return;
    }
    
    int pos = 0;
    for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
        uint32_t eax, ebx, ecx, edx;
        do_cpuid(leaf, &eax, &ebx, &ecx, &edx);
        
        uint32_t values[4] = {eax, ebx, ecx, edx};
        for (int i = 0; i < 4; i++) {
            buf[pos++] = values[i] & 0xFF;
            buf[pos++] = (values[i] >> 8) & 0xFF;
            buf[pos++] = (values[i] >> 16) & 0xFF;
            buf[pos++] = (values[i] >> 24) & 0xFF;
        }
    }
    buf[48] = '\0';

    int start = 0;
    while (buf[start] == ' ' && buf[start] != '\0') start++;
    if (start > 0) {
        for (int i = 0; buf[start + i] != '\0'; i++) {
            buf[i] = buf[start + i];
        }
        buf[pos - start] = '\0';
    }
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
    cpuid_get_brand(brand);
    char line[64];
    memset(line, 0, 64);
    strcpy(line, "Brand: ");
    strcat(line, brand);
    print_line_scroll(line, 0, row, 0x0F);
}

static void cpuid_show_vendor(int* row) {
    char vendor[13];
    cpuid_get_vendor(vendor);
    char line[32];
    memset(line, 0, 32);
    strcpy(line, "Vendor: ");
    strcat(line, vendor);
    print_line_scroll(line, 0, row, 0x0F);
}

static void cpuid_show_family(int* row) {
    uint32_t eax, ebx, ecx, edx;
    do_cpuid(1, &eax, &ebx, &ecx, &edx);
    
    uint32_t stepping = eax & 0xF;
    uint32_t base_model = (eax >> 4) & 0xF;
    uint32_t base_family = (eax >> 8) & 0xF;
    uint32_t ext_model = (eax >> 16) & 0xF;
    uint32_t ext_family = (eax >> 20) & 0xFF;
    
    uint32_t family = base_family;
    if (base_family == 0xF) {
        family = base_family + ext_family;
    }
    
    uint32_t model = base_model;
    if (base_family == 0x6 || base_family == 0xF) {
        model = (ext_model << 4) | base_model;
    }
    
    char buf[16];
    char line[64];
    
    memset(line, 0, 64);
    strcpy(line, "Family: ");
    itoa(family, buf);
    strcat(line, buf);
    print_line_scroll(line, 0, row, 0x0F);
    
    memset(line, 0, 64);
    strcpy(line, "Model: ");
    itoa(model, buf);
    strcat(line, buf);
    print_line_scroll(line, 0, row, 0x0F);
    
    memset(line, 0, 64);
    strcpy(line, "Stepping: ");
    itoa(stepping, buf);
    strcat(line, buf);
    print_line_scroll(line, 0, row, 0x0F);
}

static void cpuid_show_features(int* row) {
    uint32_t eax, ebx, ecx, edx;
    do_cpuid(1, &eax, &ebx, &ecx, &edx);
    
    print_line_scroll("Features:", 0, row, 0x0E);

    typedef struct { int bit; const char* name; } feat_t;
    
    feat_t edx_features[] = {
        {0, "FPU"}, {4, "TSC"}, {5, "MSR"}, {6, "PAE"},
        {8, "CMPXCHG8B"}, {11, "SEP"}, {15, "CMOV"},
        {19, "CLFSH"}, {23, "MMX"}, {24, "FXSR"},
        {25, "SSE"}, {26, "SSE2"}, {28, "HT"}
    };
    
    feat_t ecx_features[] = {
        {0, "SSE3"}, {1, "PCLMULQDQ"}, {9, "SSSE3"},
        {12, "FMA"}, {19, "SSE4.1"}, {20, "SSE4.2"},
        {25, "AES"}, {26, "XSAVE"}, {28, "AVX"},
        {29, "F16C"}, {30, "RDRAND"}
    };
    
    char line[80];
    int count;

    memset(line, 0, 80);
    strcpy(line, "  ");
    count = 0;
    for (int i = 0; i < 13; i++) {
        if (edx & (1 << edx_features[i].bit)) {
            if (count > 0 && strlen(line) + strlen(edx_features[i].name) + 2 < 78) {
                strcat(line, " ");
            }
            if (strlen(line) + strlen(edx_features[i].name) + 2 < 78) {
                strcat(line, edx_features[i].name);
                count++;
            }
        }
    }
    if (count > 0) print_line_scroll(line, 0, row, 0x0A);

    memset(line, 0, 80);
    strcpy(line, "  ");
    count = 0;
    for (int i = 0; i < 11; i++) {
        if (ecx & (1 << ecx_features[i].bit)) {
            if (count > 0 && strlen(line) + strlen(ecx_features[i].name) + 2 < 78) {
                strcat(line, " ");
            }
            if (strlen(line) + strlen(ecx_features[i].name) + 2 < 78) {
                strcat(line, ecx_features[i].name);
                count++;
            }
        }
    }
    if (count > 0) print_line_scroll(line, 0, row, 0x0A);

    uint32_t max_ext;
    do_cpuid(0x80000000, &max_ext, &ebx, &ecx, &edx);
    if (max_ext >= 0x80000001) {
        do_cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
        memset(line, 0, 80);
        strcpy(line, "  ");
        count = 0;
        
        if (edx & (1 << 29)) { strcat(line, "x86-64"); count++; }
        if (edx & (1 << 27)) { if (count>0) strcat(line, " "); strcat(line, "RDTSCP"); count++; }
        if (edx & (1 << 20)) { if (count>0) strcat(line, " "); strcat(line, "NX"); count++; }
        if (ecx & (1 << 5))  { if (count>0) strcat(line, " "); strcat(line, "LZCNT"); count++; }
        if (ecx & (1 << 8))  { if (count>0) strcat(line, " "); strcat(line, "3DNow!Pref"); count++; }
        if (ecx & (1 << 31)) { if (count>0) strcat(line, " "); strcat(line, "3DNow!"); count++; }
        
        if (count > 0) print_line_scroll(line, 0, row, 0x0A);
    }
}

static void cpuid_show_cache(int* row) {
    uint32_t eax, ebx, ecx, edx;
    print_line_scroll("Cache:", 0, row, 0x0E);
    uint32_t max_leaf;
    do_cpuid(0, &max_leaf, &ebx, &ecx, &edx);
    
    if (max_leaf >= 4) {
        for (int i = 0; i < 16; i++) {
            do_cpuid(4 | (i << 22), &eax, &ebx, &ecx, &edx);
        }
    }
    for (int i = 0; i < 10; i++) {
        uint32_t sub_eax = 4;
        __asm__ volatile("cpuid"
            : "=a"(sub_eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(4), "c"(i)
        );
        
        uint32_t cache_type = sub_eax & 0x1F;
        if (cache_type == 0) break;
        
        uint32_t cache_level = (sub_eax >> 5) & 0x7;
        uint32_t ways = ((ebx >> 22) & 0x3FF) + 1;
        uint32_t partitions = ((ebx >> 12) & 0x3FF) + 1;
        uint32_t line_size = (ebx & 0xFFF) + 1;
        uint32_t sets = ecx + 1;
        
        uint32_t total_size = ways * partitions * line_size * sets;
        uint32_t size_kb = total_size / 1024;
        
        const char* type_str;
        switch (cache_type) {
            case 1: type_str = "Data"; break;
            case 2: type_str = "Instruction"; break;
            case 3: type_str = "Unified"; break;
            default: type_str = "Unknown"; break;
        }
        
        char line[64];
        char buf[16];
        memset(line, 0, 64);
        strcpy(line, "  L");
        itoa(cache_level, buf);
        strcat(line, buf);
        strcat(line, " ");
        strcat(line, type_str);
        strcat(line, " Cache: ");
        itoa(size_kb, buf);
        strcat(line, buf);
        strcat(line, " KB");
        print_line_scroll(line, 0, row, 0x0F);
    }
}

static void cpuid_show_hypervisor(int* row) {
    uint32_t eax, ebx, ecx, edx;
    do_cpuid(1, &eax, &ebx, &ecx, &edx);
    
    if (!(ecx & (1 << 31))) {
        print_line_scroll("Hypervisor: None (bare metal)", 0, row, 0x0F);
        return;
    }
    
    print_line_scroll("Hypervisor: Detected", 0, row, 0x0E);
    
    do_cpuid(0x40000000, &eax, &ebx, &ecx, &edx);
    
    char sig[13];
    sig[0] = ebx & 0xFF; sig[1] = (ebx >> 8) & 0xFF;
    sig[2] = (ebx >> 16) & 0xFF; sig[3] = (ebx >> 24) & 0xFF;
    sig[4] = ecx & 0xFF; sig[5] = (ecx >> 8) & 0xFF;
    sig[6] = (ecx >> 16) & 0xFF; sig[7] = (ecx >> 24) & 0xFF;
    sig[8] = edx & 0xFF; sig[9] = (edx >> 8) & 0xFF;
    sig[10] = (edx >> 16) & 0xFF; sig[11] = (edx >> 24) & 0xFF;
    sig[12] = '\0';
    
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
    memset(line, 0, 64);
    strcpy(line, "Type: ");
    strcat(line, hv_name);
    print_line_scroll(line, 0, row, 0x0F);

    char buf[16];
    memset(line, 0, 64);
    strcpy(line, "Max hypervisor leaf: 0x");
    itoa_hex(eax, buf);
    strcat(line, buf);
    print_line_scroll(line, 0, row, 0x0F);
}

static void cpuid_show_raw(const char* arg, int* row) {
    if (arg == NULL || arg[0] == '\0') {
        print_line_scroll("Usage: cpuid --raw <leaf>", 0, row, 0x0C);
        print_line_scroll("Example: cpuid --raw 1", 0, row, 0x0E);
        return;
    }

    uint32_t leaf = 0;
    int is_hex = 0;
    
    if (arg[0] == '0' && (arg[1] == 'x' || arg[1] == 'X')) {
        is_hex = 1;
        arg += 2;
    }
    
    while (*arg) {
        leaf *= is_hex ? 16 : 10;
        if (*arg >= '0' && *arg <= '9') {
            leaf += *arg - '0';
        } else if (is_hex && *arg >= 'a' && *arg <= 'f') {
            leaf += *arg - 'a' + 10;
        } else if (is_hex && *arg >= 'A' && *arg <= 'F') {
            leaf += *arg - 'A' + 10;
        } else {
            print_line_scroll("Invalid leaf number", 0, row, 0x0C);
            return;
        }
        arg++;
    }
    
    uint32_t eax, ebx, ecx, edx;
    do_cpuid(leaf, &eax, &ebx, &ecx, &edx);
    
    char line[64];
    char buf[16];
    
    memset(line, 0, 64);
    strcpy(line, "CPUID Leaf 0x");
    itoa_hex(leaf, buf);
    strcat(line, buf);
    strcat(line, ":");
    print_line_scroll(line, 0, row, 0x0E);
    
    memset(line, 0, 64);
    strcpy(line, "  EAX: 0x");
    itoa_hex(eax, buf);
    strcat(line, buf);
    print_line_scroll(line, 0, row, 0x0F);
    
    memset(line, 0, 64);
    strcpy(line, "  EBX: 0x");
    itoa_hex(ebx, buf);
    strcat(line, buf);
    print_line_scroll(line, 0, row, 0x0F);
    
    memset(line, 0, 64);
    strcpy(line, "  ECX: 0x");
    itoa_hex(ecx, buf);
    strcat(line, buf);
    print_line_scroll(line, 0, row, 0x0F);
    
    memset(line, 0, 64);
    strcpy(line, "  EDX: 0x");
    itoa_hex(edx, buf);
    strcat(line, buf);
    print_line_scroll(line, 0, row, 0x0F);
}

static void cpuid_show_topology(int* row) {
    uint32_t eax, ebx, ecx, edx;
    
    print_line_scroll("CPU Topology:", 0, row, 0x0E);
    uint32_t max_leaf;
    do_cpuid(0, &max_leaf, &ebx, &ecx, &edx);
    
    if (max_leaf >= 0xB) {
        uint32_t threads_per_core = 0;
        uint32_t cores = 0;
        
        do_cpuid(0xB, &eax, &ebx, &ecx, &edx);
        
        if (ebx != 0) {
            threads_per_core = ebx & 0xFFFF;

            do_cpuid(0xB | (1 << 8), &eax, &ebx, &ecx, &edx);
            uint32_t total_threads = ebx & 0xFFFF;
            cores = total_threads / threads_per_core;
            
            char line[64];
            char buf[16];
            
            memset(line, 0, 64);
            strcpy(line, "  Cores: ");
            itoa(cores, buf);
            strcat(line, buf);
            print_line_scroll(line, 0, row, 0x0F);
            
            memset(line, 0, 64);
            strcpy(line, "  Threads per core: ");
            itoa(threads_per_core, buf);
            strcat(line, buf);
            print_line_scroll(line, 0, row, 0x0F);
            
            memset(line, 0, 64);
            strcpy(line, "  Total threads: ");
            itoa(total_threads, buf);
            strcat(line, buf);
            print_line_scroll(line, 0, row, 0x0F);
        } else {
            print_line_scroll("  Topology info not available", 0, row, 0x0F);
        }
    } else {
        do_cpuid(1, &eax, &ebx, &ecx, &edx);
        uint32_t logical_cpus = (ebx >> 16) & 0xFF;
        
        char line[64];
        char buf[16];
        memset(line, 0, 64);
        strcpy(line, "  Logical processors: ");
        itoa(logical_cpus, buf);
        strcat(line, buf);
        print_line_scroll(line, 0, row, 0x0F);
        print_line_scroll("  (Extended topology not supported)", 0, row, 0x07);
    }
}

static void cpuid_show_speed(int* row) {
    print_line_scroll("Measuring CPU speed...", 0, row, 0x0E);
    uint32_t tsc_lo1, tsc_hi1, tsc_lo2, tsc_hi2;
    __asm__ volatile("rdtsc" : "=a"(tsc_lo1), "=d"(tsc_hi1));
    sleep_ms(50);
    __asm__ volatile("rdtsc" : "=a"(tsc_lo2), "=d"(tsc_hi2));
    uint32_t elapsed = tsc_lo2 - tsc_lo1;
    if (tsc_hi2 != tsc_hi1) {
        print_line_scroll("CPU too fast to measure!", 0, row, 0x0C);
        return;
    }
    uint32_t freq_mhz = elapsed / 50000;
    char line[64];
    char buf[16];
    memset(line, 0, 64);
    strcpy(line, "CPU Speed: ~");
    itoa(freq_mhz, buf);
    strcat(line, buf);
    strcat(line, " MHz");
    print_line_scroll(line, 0, row, 0x0F);
    memset(line, 0, 64);
    strcpy(line, "Cycles in 50ms: ");
    itoa(elapsed, buf);
    strcat(line, buf);
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
    if (args == NULL || args[0] == '\0') {
        cpuid_show_full(row);
        return;
    }
    
    while (*args == ' ') args++;
    
    if (strcasecmp(args, "--help") == 0 || strcasecmp(args, "-h") == 0) {
        cpuid_help(row);
    } else if (strcasecmp(args, "--brand") == 0) {
        cpuid_show_brand(row);
    } else if (strcasecmp(args, "--vendor") == 0) {
        cpuid_show_vendor(row);
    } else if (strcasecmp(args, "--family") == 0) {
        cpuid_show_family(row);
    } else if (strcasecmp(args, "--features") == 0) {
        cpuid_show_features(row);
    } else if (strcasecmp(args, "--cache") == 0) {
        cpuid_show_cache(row);
    } else if (strcasecmp(args, "--hypervisor") == 0) {
        cpuid_show_hypervisor(row);
    } else if (strcasecmp(args, "--topology") == 0) {
        cpuid_show_topology(row);
    } else if (strcasecmp(args, "--speed") == 0) {
        cpuid_show_speed(row);
    } else if (strncasecmp(args, "--raw", 5) == 0) {
        char* leaf_arg = args + 5;
        while (*leaf_arg == ' ') leaf_arg++;
        cpuid_show_raw(leaf_arg, row);
    } else {
        char line[64];
        memset(line, 0, 64);
        strcpy(line, "Unknown option: ");
        strcat(line, args);
        print_line_scroll(line, 0, row, 0x0C);
        print_line_scroll("Use 'cpuid --help' for available options", 0, row, 0x0E);
    }
}

REGISTER_COMMAND("cpuid", cmd_cpuid, 1);