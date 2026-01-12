import os

def generate_index(input_dir, output_file):
    files = sorted([f for f in os.listdir(input_dir) if f.endswith('.md')])
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write("Main Page,_main.md\n")
        for filename in files:
            if filename == "_main.md": continue
            display_name = os.path.splitext(filename)[0].replace('_', ' ').title()
            f.write(f"{display_name},{filename}\n")

generate_index("markdown/", "markdown/archives.txt")