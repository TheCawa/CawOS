#ifndef CAWOS_LIBC_CPUID_H
#define CAWOS_LIBC_CPUID_H
#define CPUID_VENDOR_LEN 13
#define CPUID_BRAND_LEN  49
#define CPUID_ANY 0xFFFFFFFFu
#define CPUID_BRAND_NOT_FOUND    0
#define CPUID_BRAND_FOUND_NATIVE 1
#define CPUID_BRAND_FOUND_LEGACY 2

typedef struct {
    const char* vendor;
    unsigned int family;
    unsigned int model;
    unsigned int stepping;
    const char* brand;
} cpuid_legacy_brand_entry;

extern const cpuid_legacy_brand_entry cpuid_legacy_brand_table[];
void cpuid_raw(
    unsigned int leaf,
    unsigned int subleaf,
    unsigned int* eax,
    unsigned int* ebx,
    unsigned int* ecx,
    unsigned int* edx
);

unsigned int cpuid_max_basic(void);
unsigned int cpuid_max_extended(void);
void cpuid_get_vendor(char* buf);
void cpuid_get_family_model_stepping(
    unsigned int* family,
    unsigned int* model,
    unsigned int* stepping
);

const char* cpuid_lookup_legacy_brand(void);
int cpuid_get_brand(char* buf);
int cpuid_is_hypervisor(void);
void cpuid_hypervisor_signature(char* buf);
void get_cpu_info(char* out_str);

#endif