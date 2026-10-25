#include "libc/cpuid.h"
#include "libc/string.h"

const cpuid_legacy_brand_entry cpuid_legacy_brand_table[] = {
    { "GenuineIntel", 4,  CPUID_ANY, CPUID_ANY, "Intel 486 family" },
    { "GenuineIntel", 5,  0x01, CPUID_ANY, "Intel Pentium 60/66" },
    { "GenuineIntel", 5,  0x02, CPUID_ANY, "Intel Pentium (P54C)" },
    { "GenuineIntel", 5,  0x04, CPUID_ANY, "Intel Pentium MMX (P55C)" },
    { "GenuineIntel", 6,  0x00, CPUID_ANY, "Intel Pentium Pro (A-step)" },
    { "GenuineIntel", 6,  0x01, CPUID_ANY, "Intel Pentium Pro" },
    { "GenuineIntel", 6,  0x03, CPUID_ANY, "Intel Pentium II (Klamath)" },
    { "GenuineIntel", 6,  0x05, CPUID_ANY, "Intel Pentium II (Deschutes) / Celeron (Covington) / Mobile Pentium II" },
    { "GenuineIntel", 6,  0x06, CPUID_ANY, "Intel Celeron (Mendocino) / Mobile Pentium II" },
    { "GenuineIntel", 6,  0x07, CPUID_ANY, "Intel Pentium III (Katmai)" },
    { "GenuineIntel", 6,  0x08, CPUID_ANY, "Intel Pentium III (Coppermine) / Celeron (Coppermine-128)" },
    { "GenuineIntel", 6,  0x0A, CPUID_ANY, "Intel Pentium III-S (Tualatin)" },
    { "GenuineIntel", 6,  0x0B, CPUID_ANY, "Intel Pentium III-M (Tualatin)" },
    { "GenuineIntel", 6,  CPUID_ANY, CPUID_ANY, "Intel P6 family" },
    { "AuthenticAMD", 4,  0x0E, CPUID_ANY, "AMD Am5x86-WT" },
    { "AuthenticAMD", 4,  0x0F, CPUID_ANY, "AMD Am5x86-WB" },
    { "AuthenticAMD", 5,  0x00, CPUID_ANY, "AMD K5 (SSA5)" },
    { "AuthenticAMD", 5,  0x01, CPUID_ANY, "AMD K5" },
    { "AuthenticAMD", 5,  0x02, CPUID_ANY, "AMD K5" },
    { "AuthenticAMD", 5,  0x03, CPUID_ANY, "AMD K5" },
    { "AuthenticAMD", 5,  0x06, CPUID_ANY, "AMD K6" },
    { "AuthenticAMD", 5,  0x07, CPUID_ANY, "AMD K6" },
    { "AuthenticAMD", 5,  0x08, CPUID_ANY, "AMD K6-2" },
    { "AuthenticAMD", 5,  0x09, CPUID_ANY, "AMD K6-III" },
    { "AuthenticAMD", 5,  0x0D, CPUID_ANY, "AMD K6-2+ / K6-III+" },
    { "AuthenticAMD", 6,  0x01, CPUID_ANY, "AMD Athlon (K7)" },
    { "AuthenticAMD", 6,  0x02, CPUID_ANY, "AMD Athlon (K7)" },
    { "AuthenticAMD", 6,  0x03, CPUID_ANY, "AMD Duron (Spitfire)" },
    { "AuthenticAMD", 6,  0x04, CPUID_ANY, "AMD Athlon (Thunderbird)" },
    { "AuthenticAMD", 6,  0x06, CPUID_ANY, "AMD Athlon MP / Athlon XP (Palomino)" },
    { "AuthenticAMD", 6,  0x07, CPUID_ANY, "AMD Duron (Morgan)" },
    { "AuthenticAMD", 6,  CPUID_ANY, CPUID_ANY, "AMD K7 family" },
    { "CyrixInstead", 4,  0x04, CPUID_ANY, "Cyrix MediaGX" },
    { "CyrixInstead", 5,  0x02, CPUID_ANY, "Cyrix 6x86 / 6x86L" },
    { "CyrixInstead", 6,  0x00, CPUID_ANY, "Cyrix 6x86MX" },
    { "CyrixInstead", 6,  0x01, CPUID_ANY, "Cyrix MII" },
    { "CentaurHauls", 5,  0x04, CPUID_ANY, "IDT WinChip C6" },
    { "CentaurHauls", 5,  0x08, CPUID_ANY, "IDT WinChip 2 / 2-3D" },
    { "CentaurHauls", 6,  0x06, CPUID_ANY, "VIA C3 (Samuel)" },
    { "CentaurHauls", 6,  0x07, CPUID_ANY, "VIA C3 (Samuel 2 / Ezra)" },
    { "CentaurHauls", 6,  0x09, CPUID_ANY, "VIA C3 (Nehemiah)" },
    { "CentaurHauls", 6,  CPUID_ANY, CPUID_ANY, "VIA C3 family" },
    { "RiseRiseRise", 5,  CPUID_ANY, CPUID_ANY, "Rise mP6" },
    { "GenuineTMx86", 5,  CPUID_ANY, CPUID_ANY, "Transmeta Crusoe" },
    { "GenuineTMx86", 6,  CPUID_ANY, CPUID_ANY, "Transmeta Efficeon" },
    { 0, 0, 0, 0, 0 }
};

void cpuid_raw(
    unsigned int leaf,
    unsigned int subleaf,
    unsigned int* eax,
    unsigned int* ebx,
    unsigned int* ecx,
    unsigned int* edx
) {
    unsigned int out_eax;
    unsigned int out_ebx;
    unsigned int out_ecx;
    unsigned int out_edx;

    __asm__ volatile(
        "cpuid"
        : "=a"(out_eax), "=b"(out_ebx), "=c"(out_ecx), "=d"(out_edx)
        : "a"(leaf), "c"(subleaf)
    );

    if (eax) *eax = out_eax;
    if (ebx) *ebx = out_ebx;
    if (ecx) *ecx = out_ecx;
    if (edx) *edx = out_edx;
}

unsigned int cpuid_max_basic(void) {
    unsigned int eax;

    cpuid_raw(0, 0, &eax, 0, 0, 0);

    return eax;
}

unsigned int cpuid_max_extended(void) {
    unsigned int eax;

    cpuid_raw(0x80000000u, 0, &eax, 0, 0, 0);

    if (eax < 0x80000000u) {
        return 0;
    }

    return eax;
}

static void put_le_bytes(unsigned int value, char* p) {
    p[0] = value & 0xFF;
    p[1] = (value >> 8) & 0xFF;
    p[2] = (value >> 16) & 0xFF;
    p[3] = (value >> 24) & 0xFF;
}

void cpuid_get_vendor(char* buf) {
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;

    cpuid_raw(0, 0, &eax, &ebx, &ecx, &edx);

    put_le_bytes(ebx, buf + 0);
    put_le_bytes(edx, buf + 4);
    put_le_bytes(ecx, buf + 8);

    buf[12] = '\0';
}

void cpuid_get_family_model_stepping(
    unsigned int* family,
    unsigned int* model,
    unsigned int* stepping
) {
    if (family) *family = 0;
    if (model) *model = 0;
    if (stepping) *stepping = 0;

    if (cpuid_max_basic() < 1) {
        return;
    }

    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;

    cpuid_raw(1, 0, &eax, &ebx, &ecx, &edx);

    unsigned int base_stepping = eax & 0xFu;
    unsigned int base_model = (eax >> 4) & 0xFu;
    unsigned int base_family = (eax >> 8) & 0xFu;
    unsigned int ext_model = (eax >> 16) & 0xFu;
    unsigned int ext_family = (eax >> 20) & 0xFFu;

    unsigned int final_family = base_family;

    if (base_family == 0xFu) {
        final_family = base_family + ext_family;
    }

    unsigned int final_model = base_model;

    if (base_family == 0x6u || base_family == 0xFu) {
        final_model = (ext_model << 4) | base_model;
    }

    if (family) *family = final_family;
    if (model) *model = final_model;
    if (stepping) *stepping = base_stepping;
}

static int cpuid_find_legacy_brand(const char** out_brand) {
    if (out_brand) {
        *out_brand = 0;
    }

    char vendor[CPUID_VENDOR_LEN];

    cpuid_get_vendor(vendor);

    unsigned int family;
    unsigned int model;
    unsigned int stepping;

    cpuid_get_family_model_stepping(&family, &model, &stepping);

    for (const cpuid_legacy_brand_entry* e = cpuid_legacy_brand_table;
         e->vendor != 0;
         e++) {

        if (strcmp(vendor, e->vendor) != 0) {
            continue;
        }

        if (e->family != CPUID_ANY && e->family != family) {
            continue;
        }

        if (e->model != CPUID_ANY && e->model != model) {
            continue;
        }

        if (e->stepping != CPUID_ANY && e->stepping != stepping) {
            continue;
        }

        if (out_brand) {
            *out_brand = e->brand;
        }

        return 1;
    }

    return 0;
}

const char* cpuid_lookup_legacy_brand(void) {
    const char* brand;

    if (cpuid_find_legacy_brand(&brand)) {
        return brand;
    }

    return "Unknown CPU";
}

static void normalize_brand(char* buf) {
    int start = 0;

    while (buf[start] == ' ' && buf[start] != '\0') {
        start++;
    }

    if (start > 0) {
        int i = 0;

        while (buf[start + i] != '\0') {
            buf[i] = buf[start + i];
            i++;
        }

        buf[i] = '\0';
    }

    int len = strlen(buf);

    while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\t')) {
        buf[--len] = '\0';
    }
}

static int brand_is_empty(const char* buf) {
    for (int i = 0; buf[i] != '\0'; i++) {
        if (buf[i] != ' ') {
            return 0;
        }
    }

    return 1;
}

int cpuid_get_brand(char* buf) {
    if (buf == 0) {
        return CPUID_BRAND_NOT_FOUND;
    }

    buf[0] = '\0';

    unsigned int max_ext = cpuid_max_extended();

    if (max_ext >= 0x80000004u) {
        int pos = 0;

        for (unsigned int leaf = 0x80000002u; leaf <= 0x80000004u; leaf++) {
            unsigned int eax;
            unsigned int ebx;
            unsigned int ecx;
            unsigned int edx;

            cpuid_raw(leaf, 0, &eax, &ebx, &ecx, &edx);

            put_le_bytes(eax, buf + pos);
            pos += 4;

            put_le_bytes(ebx, buf + pos);
            pos += 4;

            put_le_bytes(ecx, buf + pos);
            pos += 4;

            put_le_bytes(edx, buf + pos);
            pos += 4;
        }

        buf[48] = '\0';

        normalize_brand(buf);

        if (!brand_is_empty(buf)) {
            return CPUID_BRAND_FOUND_NATIVE;
        }
    }

    const char* legacy;

    if (cpuid_find_legacy_brand(&legacy)) {
        safe_strcpy(buf, legacy, CPUID_BRAND_LEN);
        return CPUID_BRAND_FOUND_LEGACY;
    }

    safe_strcpy(buf, "Unknown CPU", CPUID_BRAND_LEN);
    return CPUID_BRAND_NOT_FOUND;
}

int cpuid_is_hypervisor(void) {
    unsigned int max_basic = cpuid_max_basic();

    if (max_basic < 1) {
        return 0;
    }

    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;

    cpuid_raw(1, 0, &eax, &ebx, &ecx, &edx);

    return (ecx & (1u << 31)) != 0;
}

void cpuid_hypervisor_signature(char* buf) {
    if (!cpuid_is_hypervisor()) {
        buf[0] = '\0';
        return;
    }

    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;

    cpuid_raw(0x40000000u, 0, &eax, &ebx, &ecx, &edx);

    put_le_bytes(ebx, buf + 0);
    put_le_bytes(ecx, buf + 4);
    put_le_bytes(edx, buf + 8);

    buf[12] = '\0';
}

void get_cpu_info(char* out_str) {
    unsigned int regs[4];

    cpuid_raw(0x80000000u, 0, &regs[0], 0, 0, 0);

    if (regs[0] >= 0x80000004u) {
        for (unsigned int i = 0; i < 3; i++) {
            cpuid_raw(
                0x80000002u + i,
                0,
                &regs[0],
                &regs[1],
                &regs[2],
                &regs[3]
            );

            ((unsigned int*)out_str)[i * 4 + 0] = regs[0];
            ((unsigned int*)out_str)[i * 4 + 1] = regs[1];
            ((unsigned int*)out_str)[i * 4 + 2] = regs[2];
            ((unsigned int*)out_str)[i * 4 + 3] = regs[3];
        }

        out_str[47] = '\0';
    } else {
        cpuid_raw(0, 0, &regs[0], &regs[1], &regs[2], &regs[3]);

        ((unsigned int*)out_str)[0] = regs[1];
        ((unsigned int*)out_str)[1] = regs[3];
        ((unsigned int*)out_str)[2] = regs[2];

        out_str[12] = '\0';
    }
}