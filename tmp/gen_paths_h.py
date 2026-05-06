#!/usr/bin/env python3
"""Convert a paths.txt file to a C header with an embedded string constant."""
import sys

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.txt> <output.h>", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], 'r') as f:
        content = f.read()

    escaped = (content
               .replace('\\', '\\\\')
               .replace('"', '\\"')
               .replace('\n', '\\n"\n    "'))

    with open(sys.argv[2], 'w') as f:
        f.write("// auto-generated from paths.txt — do not edit\n")
        f.write("static const char paths_data[] =\n")
        f.write(f'    "{escaped}";\n')

if __name__ == "__main__":
    main()
