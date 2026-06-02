import os
import struct
import sys

SECTOR_SIZE = 512
FILE_STRUCT_SIZE = 76
STRUCT_FORMAT = '<32s32sHHIBBBB'

def parse_fs_config(config_path="include/fs_config.h"):
    config = {}
    if not os.path.exists(config_path):
        print(f"Error: {config_path} not found!")
        return None
    
    with open(config_path, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith('#define'):
                parts = line.split()
                if len(parts) >= 3:
                    key = parts[1]
                    value = parts[2]
                    try:
                        config[key] = int(value)
                    except ValueError:
                        pass
    return config

def unpack_image(image_path="CawOS.img", output_dir="extracted", config_path="include/fs_config.h"):
    config = parse_fs_config(config_path)
    if not config:
        return False
    FS_TABLE_LBA = config.get('FS_TABLE_LBA', 591)
    FS_TABLE_SECTORS = config.get('FS_TABLE_SECTORS', 4)
    CAWFAT_SECTORS = config.get('CAWFAT_SECTORS', 1)
    DATA_REGION_START = config.get('DATA_REGION_START', 596)
    MAX_FILES = config.get('MAX_FILES', 24)
    MAX_CLUSTERS = config.get('MAX_CLUSTERS', 256)
    print(f"Config loaded from {config_path}:")
    print(f"  FS_TABLE_LBA: {FS_TABLE_LBA}")
    print(f"  FS_TABLE_SECTORS: {FS_TABLE_SECTORS}")
    print(f"  CAWFAT_SECTORS: {CAWFAT_SECTORS}")
    print(f"  DATA_REGION_START: {DATA_REGION_START}")
    print(f"  MAX_FILES: {MAX_FILES}")
    print(f"  MAX_CLUSTERS: {MAX_CLUSTERS}")
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    print(f"\nOpening image to extract (CawFS v2): {image_path}")
    with open(image_path, 'rb') as f:
        f.seek(FS_TABLE_LBA * SECTOR_SIZE)
        table_data = f.read(MAX_FILES * FILE_STRUCT_SIZE)
        cawfat_lba = FS_TABLE_LBA + FS_TABLE_SECTORS
        f.seek(cawfat_lba * SECTOR_SIZE)
        cawfat_data = f.read(MAX_CLUSTERS * 2)
        cawfat = list(struct.unpack(f'<{MAX_CLUSTERS}H', cawfat_data))
        entries = []
        for i in range(MAX_FILES):
            offset = i * FILE_STRUCT_SIZE
            raw_entry = table_data[offset : offset + FILE_STRUCT_SIZE]
            try:
                entry = struct.unpack(STRUCT_FORMAT, raw_entry)
            except struct.error:
                continue

            name_raw = entry[0]
            dir_raw = entry[1]
            first_cluster = entry[2]
            size_bytes = entry[4]
            exists = entry[6]
            is_dir = entry[7]
            if exists == 1:
                name = name_raw.decode('ascii', errors='ignore').strip('\x00 ')
                directory = dir_raw.decode('ascii', errors='ignore').strip('\x00 ')
                if directory == "/":
                    full_path = name
                else:
                    clean_dir = directory.lstrip('/')
                    full_path = os.path.join(clean_dir, name) if clean_dir else name
                entries.append({
                    'name': name,
                    'full_path': full_path,
                    'first_cluster': first_cluster,
                    'size': size_bytes,
                    'is_dir': is_dir
                })
        print(f"\nFound {len(entries)} entries:")
        for item in entries:
            item_type = "DIR" if item['is_dir'] else "FILE"
            print(f"  {item_type}: {item['full_path']} (cluster: {item['first_cluster']}, size: {item['size']})")
        print(f"\nExtracting...")
        for item in entries:
            if item['is_dir']:
                target_folder = os.path.join(output_dir, item['full_path'])
                if not os.path.exists(target_folder):
                    os.makedirs(target_folder)
                    print(f"  Created Dir: {target_folder}")
        for item in entries:
            if not item['is_dir']:
                parent_dir = os.path.dirname(item['full_path'])
                file_name = os.path.basename(item['full_path'])
                target_dir = os.path.join(output_dir, parent_dir) if parent_dir else output_dir
                if not os.path.exists(target_dir):
                    os.makedirs(target_dir)  
                out_path = os.path.join(target_dir, file_name)
                file_data = b''
                curr_cluster = item['first_cluster']
                bytes_left = item['size']
                while curr_cluster != 0xFFFF and bytes_left > 0 and curr_cluster < MAX_CLUSTERS:
                    f.seek((DATA_REGION_START + curr_cluster) * SECTOR_SIZE)
                    chunk = f.read(min(bytes_left, SECTOR_SIZE))
                    file_data += chunk
                    bytes_left -= len(chunk)
                    curr_cluster = cawfat[curr_cluster]
                with open(out_path, 'wb') as out_file:
                    out_file.write(file_data)
                print(f"  Extracted: {out_path} ({item['size']} bytes)")
    print("\nUnpacking completed successfully!")
    return True

if __name__ == "__main__":
    image_path = sys.argv[1] if len(sys.argv) > 1 else "CawOS.img"
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "extracted"
    config_path = sys.argv[3] if len(sys.argv) > 3 else "include/fs_config.h"
    unpack_image(image_path, output_dir, config_path)