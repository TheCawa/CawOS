#include "kernel/elf.h"
#include "fs.h"
#include "libc/util.h"
#include "drivers/screen.h"

#define ELF_LOAD_BUFFER_ADDR 0x00500000
static uint8_t* elf_buffer = (uint8_t*)ELF_LOAD_BUFFER_ADDR;

elf_result_t elf_load(const char* path, uint32_t load_base) {
    elf_result_t res = {0};
    char* path_mut = (char*)path;
    uint32_t fsize = fs_get_size(path_mut);
    if (fsize == 0 || fsize > 0x200000) {
        strcpy(res.error, "Invalid file size");
        return res;
    }
    memset(elf_buffer, 0, fsize < 0x10000 ? 0x10000 : fsize);
    if (fs_load_to_memory(path_mut, elf_buffer) == 0) {
        strcpy(res.error, "Read failed");
        return res;
    }
    elf_header_t* hdr = (elf_header_t*)elf_buffer;
    if (memcmp(hdr->e_ident, ELF_MAGIC, 4) != 0) {
        strcpy(res.error, "Not an ELF file");
        return res;
    }
    if (hdr->e_ident[4] != ELF_CLASS_32 || hdr->e_ident[5] != ELF_DATA_LSB) {
        strcpy(res.error, "Unsupported ELF class/data");
        return res;
    }
    if (hdr->e_type != ELF_TYPE_EXEC || hdr->e_machine != ELF_MACHINE_I386) {
        strcpy(res.error, "Unsupported ELF type/machine");
        return res;
    }
    elf_program_header_t* phdrs = (elf_program_header_t*)(elf_buffer + hdr->e_phoff);
    uint32_t min_vaddr = 0xFFFFFFFF;
    for (int i = 0; i < hdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD && phdrs[i].p_vaddr < min_vaddr) {
            min_vaddr = phdrs[i].p_vaddr;
        }
    }
    uint32_t offset = 0;
    if (hdr->e_entry < 0x800000) {
        offset = load_base - min_vaddr;
    }
    for (int i = 0; i < hdr->e_phnum; i++) {
        elf_program_header_t* ph = &phdrs[i];
        if (ph->p_type != PT_LOAD) continue;
        if (ph->p_offset + ph->p_filesz > fsize) {
            strcpy(res.error, "Segment out of bounds");
            return res;
        }
        uint8_t* dest = (uint8_t*)(ph->p_vaddr + offset);
        memcpy(dest, elf_buffer + ph->p_offset, ph->p_filesz);
        if (ph->p_memsz > ph->p_filesz) {
            memset(dest + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
        }
    }
    res.success = 1;
    res.entry = hdr->e_entry + offset;
    return res;
}
