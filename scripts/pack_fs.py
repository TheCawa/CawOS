import os
import struct

ROOT_DIR_LBA = 65
DATA_START_LBA = 200
SECTOR_SIZE = 512
FS_SECTORS = 4

def pack_fs(image_path, bin_dir):
    files = [f for f in os.listdir(bin_dir) if f.endswith('.bin')]
    entries = b''
    current_lba = DATA_START_LBA
    for f_name in files:
        path = os.path.join(bin_dir, f_name)
        size = os.path.getsize(path)
        name = f_name.replace('.bin', '').encode('ascii')
        entry = struct.pack('32s32sIIBBBB',
                            name,
                            b'/',        # dir
                            current_lba,
                            size,
                            1,           # is_executable
                            1,           # exists
                            0,           # is_dir
                            0)           # reserved
        entries += entry
        
        content_size = size
        if content_size % SECTOR_SIZE != 0:
            content_size += SECTOR_SIZE - (content_size % SECTOR_SIZE)
        current_lba += content_size // SECTOR_SIZE

    entries += b'\x00' * (SECTOR_SIZE * FS_SECTORS - len(entries))
    with open(image_path, 'r+b') as img:
        img.seek(ROOT_DIR_LBA * SECTOR_SIZE)
        img.write(entries)
        img.seek(DATA_START_LBA * SECTOR_SIZE)
        for f_name in files:
            with open(os.path.join(bin_dir, f_name), 'rb') as f:
                content = f.read()
                img.write(content)
                if len(content) % SECTOR_SIZE != 0:
                    img.write(b'\x00' * (SECTOR_SIZE - (len(content) % SECTOR_SIZE)))

pack_fs('os-image.bin', 'build/apps/')