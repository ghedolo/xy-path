#!/usr/bin/env python3
"""Convert one or more paths files to a C header with an embedded string constant."""
import sys

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <output.h> <input1.txt> [input2.txt ...]", file=sys.stderr)
        sys.exit(1)

    output = sys.argv[1]
    inputs = sys.argv[2:]

    content = ""
    for path in inputs:
        with open(path, 'r') as f:
            lines = [l for l in f if not l.lstrip().startswith('#')]
        content += "".join(lines)
        if not content.endswith('\n'):
            content += '\n'

    escaped = (content
               .replace('\\', '\\\\')
               .replace('"', '\\"')
               .replace('\n', '\\n"\n    "'))

    with open(output, 'w') as f:
        f.write("// auto-generated — do not edit\n")
        f.write("static const char paths_data[] =\n")
        f.write(f'    "{escaped}";\n')

if __name__ == "__main__":
    main()
