import sys

image_path = sys.argv[1]
boot_path = sys.argv[2]
kernel_path = sys.argv[3]

with open(image_path, 'r+b') as img:
    with open(boot_path, 'rb') as f:
        img.write(f.read())
    with open(kernel_path, 'rb') as f:
        img.write(f.read())