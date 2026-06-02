import sys, os, subprocess

SECTOR_SIZE = 512
STAGE2_START_LBA = 1
KERNEL_START_LBA = 5
MAX_FILES = 1024
MAX_CLUSTERS = 32768
FILE_STRUCT_SIZE = 76

def build_image(boot_bin, stage2_bin, kernel_bin, recovery_bin, apps_dir, output_img, size_mb=16):
    with open(boot_bin, 'rb') as f:
        boot_data = f.read()
    if len(boot_data) != SECTOR_SIZE:
        print("Error: Bootloader must be exactly 512 bytes!")
        return False
    with open(stage2_bin, 'rb') as f:
        stage2_data = f.read()
    with open(kernel_bin, 'rb') as f:
        kernel_data = f.read()
    with open(recovery_bin, 'rb') as f:
        recovery_data = f.read()  
    kernel_sectors = (len(kernel_data) + SECTOR_SIZE - 1) // SECTOR_SIZE
    recovery_sectors = (len(recovery_data) + SECTOR_SIZE - 1) // SECTOR_SIZE
    fs_table_bytes = MAX_FILES * FILE_STRUCT_SIZE
    fs_table_sectors = (fs_table_bytes + SECTOR_SIZE - 1) // SECTOR_SIZE
    cawfat_bytes = MAX_CLUSTERS * 2 # uint16_t
    cawfat_sectors = (cawfat_bytes + SECTOR_SIZE - 1) // SECTOR_SIZE
    recovery_start_lba = KERNEL_START_LBA + kernel_sectors + 2
    fs_table_lba   = recovery_start_lba + recovery_sectors + 10
    cawfat_lba     = fs_table_lba + fs_table_sectors
    data_start_lba = cawfat_lba + cawfat_sectors
    print(f"--- CawOS FS Разметка ---")
    print(f"MAX_FILES:      {MAX_FILES} (Занимает {fs_table_sectors} секторов)")
    print(f"MAX_CLUSTERS:   {MAX_CLUSTERS} (Занимает {cawfat_sectors} секторов)")
    print(f"FS Table LBA:   {fs_table_lba}")
    print(f"CawFAT LBA:     {cawfat_lba}")
    print(f"Data Start LBA: {data_start_lba}")
    print(f"-------------------------")
    total_bytes = size_mb * 1024 * 1024
    img = bytearray(total_bytes)
    img[0:len(boot_data)] = boot_data
    off = STAGE2_START_LBA * SECTOR_SIZE
    img[off:off+len(stage2_data)] = stage2_data
    off = KERNEL_START_LBA * SECTOR_SIZE
    img[off:off+len(kernel_data)] = kernel_data
    off = recovery_start_lba * SECTOR_SIZE
    img[off:off+len(recovery_data)] = recovery_data
    with open(output_img, 'wb') as f:
        f.write(img)   
    cmd = [
        "python", "scripts/pack_fs.py",
        output_img, apps_dir,
        str(fs_table_lba), str(data_start_lba), 
        str(fs_table_sectors), str(MAX_FILES), str(MAX_CLUSTERS)
    ]
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print("Error packing filesystem!")
        return False
    with open("include/fs_config.h", "w") as f:
        f.write(f"""#ifndef FS_CONFIG_H
#define FS_CONFIG_H

#define MAX_FILES {MAX_FILES}
#define MAX_CLUSTERS {MAX_CLUSTERS}
#define FS_TABLE_LBA {fs_table_lba}
#define FS_TABLE_SECTORS {fs_table_sectors}
#define CAWFAT_SECTORS {cawfat_sectors}
#define DATA_REGION_START {data_start_lba}

#endif
""")
    print(f"Generated fs_config.h")
    return True
if __name__ == "__main__":
    if len(sys.argv) < 7:
        print("Usage: build_img.py boot.bin stage2.bin kernel.bin recovery.bin apps_dir output.img [size_mb]")
        sys.exit(1)
    size = int(sys.argv[7]) if len(sys.argv) > 7 else 16
    success = build_image(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4],
                          sys.argv[5], sys.argv[6], size)
    sys.exit(0 if success else 1)