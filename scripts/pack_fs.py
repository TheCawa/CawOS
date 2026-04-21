import os
import struct

SUPERBLOCK_LBA = 64
ROOT_DIR_LBA = 65
DATA_START_LBA = 71
SECTOR_SIZE = 512

def pack_fs(image_path, bin_dir):
    with open(image_path, 'ab') as img:
        current_size = os.path.getsize(image_path)
        target_padding = SUPERBLOCK_LBA * SECTOR_SIZE
        if current_size < target_padding:
            img.write(b'\x00' * (target_padding - current_size))
        files = [f for f in os.listdir(bin_dir) if f.endswith('.bin')]
        file_entries = []
        current_lba = DATA_START_LBA

        for f_name in files:
            path = os.path.join(bin_dir, f_name)
            size = os.path.getsize(path)
            entry = struct.pack('32sIIBB', 
                                f_name.replace('.bin', '').encode('ascii'), 
                                current_lba, 
                                size, 
                                1, 1)
            file_entries.append(entry)
            with open(path, 'rb') as f_data:
                content = f_data.read()
                if len(content) % SECTOR_SIZE != 0:
                    content += b'\x00' * (SECTOR_SIZE - (len(content) % SECTOR_SIZE))
            current_lba += (len(content) // SECTOR_SIZE)
        img.seek(ROOT_DIR_LBA * SECTOR_SIZE)
        for entry in file_entries:
            img.write(entry)
        img.write(b'\x00' * (SECTOR_SIZE * 2 - len(file_entries) * 44))
        img.seek(DATA_START_LBA * SECTOR_SIZE)
        for f_name in files:
            with open(os.path.join(bin_dir, f_name), 'rb') as f_data:
                content = f_data.read()
                img.write(content)
                if len(content) % SECTOR_SIZE != 0:
                    img.write(b'\x00' * (SECTOR_SIZE - (len(content) % SECTOR_SIZE)))

pack_fs('os-image.bin', 'build/apps/')