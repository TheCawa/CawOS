import os
import struct
import sys

DEFAULT_FS_TABLE_LBA = 591
DEFAULT_DATA_START_LBA = 596
DEFAULT_FS_SECTORS = 4
SECTOR_SIZE = 512
MAX_FILES = 24
MAX_CLUSTERS = 256

def normalize_path(p):
    p = p.replace('\\', '/')
    if not p:
        return '/'
    if not p.startswith('/'):
        p = '/' + p
    return p

def pack_fs(image_path, bin_dir, fs_table_lba, data_start_lba, fs_sectors):
    if not os.path.exists(image_path):
        print(f"Error: Image '{image_path}' not found!")
        return False
    if not os.path.exists(bin_dir):
        print(f"Warning: Directory '{bin_dir}' not found, creating empty FS")
        files = []
    else:
        files = []
        for root, dirs, filenames in os.walk(bin_dir):
            for filename in filenames:
                full_path = os.path.join(root, filename)
                rel_path = os.path.relpath(full_path, bin_dir)
                files.append((filename, rel_path, full_path))
    print(f"Packing {len(files)} files into '{image_path}'")
    print(f"  FS Table LBA: {fs_table_lba}")
    print(f"  CawFAT LBA:   {fs_table_lba + fs_sectors}")
    print(f"  Data Start LBA: {data_start_lba}")
    entries = b''
    cawfat = [0x0000] * MAX_CLUSTERS
    current_cluster = 1
    file_clusters_map = []
    all_dirs = set()
    for f_name, rel_path, full_path in files:
        dir_path = os.path.dirname(rel_path).replace('\\', '/')
        if dir_path:
            parts = dir_path.split('/')
            for i in range(len(parts)):
                all_dirs.add('/'.join(parts[:i+1]))
    sorted_dirs = sorted(all_dirs, key=lambda x: (x.count('/'), x))
    for dir_path in sorted_dirs:
        dir_name = os.path.basename(dir_path)
        parent_dir = normalize_path(os.path.dirname(dir_path))
        name = dir_name.encode('ascii', errors='ignore')
        if len(name) > 31:
            name = name[:31]
        entry = struct.pack('<32s32sHHIBBBB',
                            name.ljust(32, b'\x00'),
                            parent_dir.encode('ascii', errors='ignore').ljust(32, b'\x00'),
                            0xFFFF,
                            0,
                            0,
                            0,
                            1,
                            1,
                            0)
        entries += entry
        print(f"  DIR: {dir_name} -> parent: {parent_dir}")
    for f_name, rel_path, full_path in files:
        size = os.path.getsize(full_path)
        name_without_ext = os.path.splitext(f_name)[0] 
        name = name_without_ext.encode('ascii', errors='ignore')
        if len(name) > 31:
            name = name[:31]
        sectors_needed = (size + SECTOR_SIZE - 1) // SECTOR_SIZE
        if sectors_needed == 0:
            first_cluster = 0xFFFF
        else:
            first_cluster = current_cluster
            if current_cluster + sectors_needed >= MAX_CLUSTERS:
                print(f"Error: Disk Image is full! Cannot fit '{f_name}'")
                return False
            for s in range(sectors_needed):
                if s == sectors_needed - 1:
                    cawfat[current_cluster] = 0xFFFF
                else:
                    cawfat[current_cluster] = current_cluster + 1
                current_cluster += 1
        file_clusters_map.append((f_name, rel_path, full_path, first_cluster, sectors_needed))
        dir_path = normalize_path(os.path.dirname(rel_path))
        entry = struct.pack('<32s32sHHIBBBB',
                            name.ljust(32, b'\x00'),
                            dir_path.encode('ascii', errors='ignore').ljust(32, b'\x00'),
                            first_cluster,
                            0,
                            size,
                            1,
                            1,
                            0,
                            0)
        entries += entry
        print(f"  FILE: {rel_path} -> dir: {dir_path}, cluster: {first_cluster} ({sectors_needed} clusters)")
    
    table_size = fs_sectors * SECTOR_SIZE
    if len(entries) > table_size:
        print(f"Warning: FS table overflow! ({len(entries)} > {table_size} bytes)")
        entries = entries[:table_size]
    else:
        entries += b'\x00' * (table_size - len(entries))
    
    cawfat_bytes = struct.pack(f'<{MAX_CLUSTERS}H', *cawfat)
    with open(image_path, 'r+b') as img:
        img.seek(fs_table_lba * SECTOR_SIZE)
        img.write(entries)
        cawfat_lba = fs_table_lba + fs_sectors
        img.seek(cawfat_lba * SECTOR_SIZE)
        img.write(cawfat_bytes)
        for f_name, rel_path, full_path, first_clst, num_clst in file_clusters_map:
            if first_clst == 0xFFFF:
                continue
            with open(full_path, 'rb') as f:
                content = f.read()
            img.seek((data_start_lba + first_clst) * SECTOR_SIZE)
            img.write(content)
            remainder = len(content) % SECTOR_SIZE
            if remainder != 0:
                img.write(b'\x00' * (SECTOR_SIZE - remainder))
    print("Packing complete successfully!")
    return True

def main():
    if len(sys.argv) < 3:
        print("Usage: pack_fs.py <image_path> <bin_directory> [fs_table_lba] [data_start_lba] [fs_sectors]")
        sys.exit(1)
    image_path = sys.argv[1]
    bin_dir = sys.argv[2]
    fs_table_lba = int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_FS_TABLE_LBA
    data_start_lba = int(sys.argv[4]) if len(sys.argv) > 4 else DEFAULT_DATA_START_LBA
    fs_sectors = int(sys.argv[5]) if len(sys.argv) > 5 else DEFAULT_FS_SECTORS
    if data_start_lba < fs_table_lba + fs_sectors + 1:
        print("Error: DATA_START_LBA must be after FS_TABLE_LBA + FS_SECTORS + 1 (CawFAT)")
        sys.exit(1)
    pack_fs(image_path, bin_dir, fs_table_lba, data_start_lba, fs_sectors)

if __name__ == "__main__":
    main()