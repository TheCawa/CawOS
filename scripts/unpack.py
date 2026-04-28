import os
import struct

SECTOR_SIZE = 512
FS_TABLE_LBA = 65
MAX_FILES = 24
FILE_STRUCT_SIZE = 76
STRUCT_FORMAT = '<32s32sIIBBBB'

def unpack_image(image_path="os-image.bin", output_dir="extracted"):
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    print(f"Opening image: {image_path}")

    with open(image_path, 'rb') as f:
        f.seek(FS_TABLE_LBA * SECTOR_SIZE)
        table_data = f.read(MAX_FILES * FILE_STRUCT_SIZE)
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
            start_lba = entry[2]
            size_bytes = entry[3]
            exists = entry[5]
            is_dir = entry[6]
            if exists == 1:
                name = name_raw.decode('ascii', errors='ignore').strip('\x00 ')
                directory = dir_raw.decode('ascii', errors='ignore').strip('\x00 ')
                if directory == "/":
                    full_path = name
                else:
                    clean_dir = directory.lstrip('/')
                    if clean_dir:
                        full_path = os.path.join(clean_dir, name)
                    else:
                        full_path = name
                
                entries.append({
                    'name': name,
                    'full_path': full_path,
                    'start_lba': start_lba,
                    'size': size_bytes,
                    'is_dir': is_dir
                })
        for item in entries:
            if item['is_dir']:
                parent_dir = item['full_path'].rsplit(os.sep, 1)[0] if os.sep in item['full_path'] else ""
                folder_name = os.path.basename(item['full_path'])
                target_folder = os.path.join(output_dir, parent_dir, folder_name) if parent_dir else os.path.join(output_dir, folder_name)
                if not os.path.exists(target_folder):
                    os.makedirs(target_folder)
                    print(f"Created Dir: {target_folder}")
        for item in entries:
            if not item['is_dir']:
                parent_dir = os.path.dirname(item['full_path'])
                file_name = os.path.basename(item['full_path'])
                target_dir = os.path.join(output_dir, parent_dir) if parent_dir else output_dir
                if not os.path.exists(target_dir):
                    os.makedirs(target_dir)
                out_path = os.path.join(target_dir, file_name)
                f.seek(item['start_lba'] * SECTOR_SIZE)
                file_data = f.read(item['size'])
                with open(out_path, 'wb') as out_file:
                    out_file.write(file_data)
                print(f"Extracted File: {out_path} ({item['size']} bytes)")

    print("Unpacking complete!")

if __name__ == "__main__":
    unpack_image()