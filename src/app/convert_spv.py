#!/usr/bin/env python3
import sys

def convert_spv_to_inl(spv_file, inl_file):
    with open(spv_file, 'rb') as f:
        data = f.read()
    
    with open(inl_file, 'w') as f:
        for i, byte in enumerate(data):
            if i % 12 == 0:
                f.write('\n    ')
            f.write(f'0x{byte:02x}, ')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f'Usage: {sys.argv[0]} <input.spv> <output.inl>')
        sys.exit(1)
    
    convert_spv_to_inl(sys.argv[1], sys.argv[2])
    print(f'Converted {sys.argv[1]} to {sys.argv[2]}')