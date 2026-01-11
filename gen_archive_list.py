import os

def generate_archives_header(input_dir, output_header):
    entries = []

    files = sorted([f for f in os.listdir(input_dir) if f.endswith('.md')])
    
    for filename in files:
        if filename == "_main.md":
            continue

        display_name = os.path.splitext(filename)[0].replace('_', ' ').title()

        entry = f'    {{.name = CLAY_STRING("{display_name}"), .path = CLAY_STRING("{filename}"), .active = FALSE}}'
        entries.append(entry)

    macro_content = "#ifndef ARCHIVES_H\n#define ARCHIVES_H\n\n"
    macro_content += "#define ARCHIVE_ENTRIES \\\n"
    macro_content += ", \\\n".join(entries)
    macro_content += "\n\n#endif"

    try:
        with open(output_header, 'w', encoding='utf-8') as f:
            f.write(macro_content)
        print(f"Successfully generated C macro file: {output_header}")
        print(f"Total indexed articles: {len(entries)}")
    except Exception as e:
        print(f"Generation failed: {e}")

if __name__ == "__main__":
    markdown_dir = "markdown/"
    header_path = "archives.h"
    
    if os.path.exists(markdown_dir):
        generate_archives_header(markdown_dir, header_path)
    else:
        print(f"Directory not found: {markdown_dir}")