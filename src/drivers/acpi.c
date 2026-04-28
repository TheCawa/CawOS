#include "acpi.h"
#include "util.h"
#include "io.h"

uint32_t g_acpiCpuCount = 0;
uint8_t  g_acpiCpuIds[MAX_CPU_COUNT];
uint8_t* g_localApicAddr = 0;
uint8_t* g_ioApicAddr    = 0;

static void* s_madt = 0;

static uint16_t s_pm1a_cnt = 0;
static uint16_t s_slp_typa = 0;
static uint16_t s_slp_en   = 1 << 13;

typedef struct {
    uint32_t signature;
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    uint8_t  oem[6];
    uint8_t  oemTableId[8];
    uint32_t oemRevision;
    uint32_t creatorId;
    uint32_t creatorRevision;
} PACKED AcpiHeader;

typedef struct {
    AcpiHeader header;
    uint32_t firmwareControl;
    uint32_t dsdt;
    uint8_t  reserved;
    uint8_t  preferredPMProfile;
    uint16_t sciInterrupt;
    uint32_t smiCommandPort;
    uint8_t  acpiEnable;
    uint8_t  acpiDisable;
    uint8_t  s4biosReq;
    uint8_t  pStateControl;
    uint32_t pm1aEventBlock;
    uint32_t pm1bEventBlock;
    uint32_t pm1aControlBlock;
    uint32_t pm1bControlBlock;
} PACKED AcpiFadt;

typedef struct {
    AcpiHeader header;
    uint32_t localApicAddr;
    uint32_t flags;
} PACKED AcpiMadt;

typedef struct {
    uint8_t type;
    uint8_t length;
} PACKED ApicHeader;

typedef struct {
    ApicHeader header;
    uint8_t acpiProcessorId;
    uint8_t apicId;
    uint32_t flags;
} PACKED ApicLocalApic;

typedef struct {
    ApicHeader header;
    uint8_t  ioApicId;
    uint8_t  reserved;
    uint32_t ioApicAddress;
    uint32_t globalSystemInterruptBase;
} PACKED ApicIoApic;

typedef struct {
    ApicHeader header;
    uint8_t  bus;
    uint8_t  source;
    uint32_t interrupt;
    uint16_t flags;
} PACKED ApicInterruptOverride;

#define APIC_TYPE_LOCAL_APIC         0
#define APIC_TYPE_IO_APIC            1
#define APIC_TYPE_INTERRUPT_OVERRIDE 2

static void parse_fadt(AcpiFadt* fadt) {
    s_pm1a_cnt = (uint16_t)fadt->pm1aControlBlock;
    s_slp_typa = 5 << 10; // S5 sleep type
}

static void parse_madt(AcpiMadt* madt) {
    s_madt = madt;
    g_localApicAddr = (uint8_t*)(uintptr_t)madt->localApicAddr;

    uint8_t* p   = (uint8_t*)(madt + 1);
    uint8_t* end = (uint8_t*)madt + madt->header.length;

    while (p < end) {
        ApicHeader* header = (ApicHeader*)p;
        uint8_t type   = header->type;
        uint8_t length = header->length;

        if (type == APIC_TYPE_LOCAL_APIC) {
            ApicLocalApic* s = (ApicLocalApic*)p;
            if (g_acpiCpuCount < MAX_CPU_COUNT) {
                g_acpiCpuIds[g_acpiCpuCount++] = s->apicId;
            }
        } else if (type == APIC_TYPE_IO_APIC) {
            ApicIoApic* s = (ApicIoApic*)p;
            g_ioApicAddr = (uint8_t*)(uintptr_t)s->ioApicAddress;
        }

        p += length;
    }
}

static void parse_dt(AcpiHeader* header) {
    uint32_t sig = header->signature;
    if (sig == 0x50434146) parse_fadt((AcpiFadt*)header); // FACP
    else if (sig == 0x43495041) parse_madt((AcpiMadt*)header); // APIC
}

static void parse_rsdt(AcpiHeader* rsdt) {
    uint32_t* p   = (uint32_t*)(rsdt + 1);
    uint32_t* end = (uint32_t*)((uint8_t*)rsdt + rsdt->length);
    while (p < end) {
        uint32_t addr = *p++;
        parse_dt((AcpiHeader*)(uintptr_t)addr);
    }
}

static void parse_xsdt(AcpiHeader* xsdt) {
    uint64_t* p   = (uint64_t*)(xsdt + 1);
    uint64_t* end = (uint64_t*)((uint8_t*)xsdt + xsdt->length);
    while (p < end) {
        uint64_t addr = *p++;
        parse_dt((AcpiHeader*)(uintptr_t)addr);
    }
}

static int parse_rsdp(uint8_t* p) {
    uint8_t sum = 0;
    for (int i = 0; i < 20; i++) sum += p[i];
    if (sum) return 0;

    uint8_t revision = p[15];
    if (revision == 0) {
        uint32_t rsdt_addr = *(uint32_t*)(p + 16);
        parse_rsdt((AcpiHeader*)(uintptr_t)rsdt_addr);
    } else if (revision == 2) {
        uint64_t xsdt_addr = *(uint64_t*)(p + 24);
        if (xsdt_addr)
            parse_xsdt((AcpiHeader*)(uintptr_t)xsdt_addr);
        else {
            uint32_t rsdt_addr = *(uint32_t*)(p + 16);
            parse_rsdt((AcpiHeader*)(uintptr_t)rsdt_addr);
        }
    }
    return 1;
}

void acpi_init() {
    uint8_t* p   = (uint8_t*)0x000e0000;
    uint8_t* end = (uint8_t*)0x000fffff;

    while (p < end) {
        uint64_t sig = *(uint64_t*)p;
        if (sig == 0x2052545020445352) { // "RSD PTR "
            if (parse_rsdp(p)) break;
        }
        p += 16;
    }
}

void acpi_shutdown() {
    if (s_pm1a_cnt) {
        port_word_out(s_pm1a_cnt, s_slp_typa | s_slp_en);
    }
    port_word_out(0x604, 0x2000);
    port_word_out(0x4004, 0x3400);
}

uint32_t acpi_remap_irq(uint32_t irq) {
    if (!s_madt) return irq;
    AcpiMadt* madt = (AcpiMadt*)s_madt;
    uint8_t* p   = (uint8_t*)(madt + 1);
    uint8_t* end = (uint8_t*)madt + madt->header.length;

    while (p < end) {
        ApicHeader* header = (ApicHeader*)p;
        if (header->type == APIC_TYPE_INTERRUPT_OVERRIDE) {
            ApicInterruptOverride* s = (ApicInterruptOverride*)p;
            if (s->source == (uint8_t)irq) return s->interrupt;
        }
        p += header->length;
    }
    return irq;
}