import sys, os
path = sys.argv[1]
size = int(sys.argv[2])
current = os.path.getsize(path)
if current < size:
    with open(path, 'ab') as f:
        f.write(b'\x00' * (size - current))
print(f"Padded {path} to {size} bytes")