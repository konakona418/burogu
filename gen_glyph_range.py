import os

def generate_glyph_range(input_dir, output_file):
    chars = set()

    for root, dirs, files in os.walk(input_dir):
        for file in files:
            if file.endswith('.md'):
                file_path = os.path.join(root, file)
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        content = f.read()
                        chars.update(content)
                except Exception as e:
                    print(f"Error reading {file_path}: {e}")

    base_chars = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
    chars.update(base_chars)

    sorted_chars = sorted([c for c in chars if ord(c) >= 32])

    try:
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write("".join(sorted_chars))
        print(f"Successfully generated! Total characters: {len(sorted_chars)}")
        print(f"File saved: {output_file}")
    except Exception as e:
        print(f"Failed to write into file: {e}")

if __name__ == "__main__":
    input_directory = "markdown/"
    output_path = os.path.join(input_directory, "glyph_range.txt")
    
    if os.path.exists(input_directory):
        generate_glyph_range(input_directory, output_path)
    else:
        print(f"Error: Directory not found {input_directory}")