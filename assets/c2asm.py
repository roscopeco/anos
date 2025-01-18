import sys
import os.path
import re

def convert_to_assembly(input_file):
    # Generate output filename by replacing extension
    output_file = os.path.splitext(input_file)[0] + '.inc'
    
    # Read the input file
    with open(input_file, 'r') as f:
        content = f.read()

    # Find the height definition
    height_match = re.search(r'#define\s+\w+_HEIGHT\s+(\d+)', content)
    if not height_match:
        raise ValueError("Could not find HEIGHT definition in header")
    
    height = int(height_match.group(1))
    num_chars = height // 8

    # Find the array data
    start = content.find('{') + 1
    end = content.find('}')
    data_str = content[start:end].strip()
    
    # Convert string of hex numbers to list of integers
    numbers = []
    for num in data_str.split(','):
        num = num.strip()
        if num:  # Skip empty strings
            try:
                if num.startswith('0x'):
                    numbers.append(int(num, 16))
                else:
                    numbers.append(int(num))
            except ValueError as e:
                print(f"Error parsing number: {num}")
                raise e

    # Generate assembly output
    assembly_lines = ['; Generated font data']
    assembly_lines.append('NUM_CHARS equ ' + str(num_chars))
    assembly_lines.append('')
    assembly_lines.append('font_data:')
    
    for num in numbers:
        # Convert to binary string, ensure 8 bits, add 'b' suffix
        binary = format(num, '08b')
        assembly_lines.append(f'    db {binary}b')

    # Write to output file
    with open(output_file, 'w') as f:
        f.write('\n'.join(assembly_lines))
    
    return output_file, num_chars

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <input_header_file>")
        sys.exit(1)
        
    input_file = sys.argv[1]
    
    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' not found")
        sys.exit(1)
        
    try:
        output_file, num_chars = convert_to_assembly(input_file)
        print(f"Conversion complete. {num_chars} characters written to {output_file}")
    except Exception as e:
        print(f"Error during conversion: {str(e)}")
        sys.exit(1)