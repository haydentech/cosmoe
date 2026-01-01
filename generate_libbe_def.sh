#!/bin/bash
# Generate a .def file for libbe.dll with stable ordinals
# This ensures binary compatibility across rebuilds

set -e

DLL_PATH="$1"
OUTPUT_DEF="$2"

if [ -z "$DLL_PATH" ] || [ -z "$OUTPUT_DEF" ]; then
    echo "Usage: $0 <path-to-libbe.dll> <output.def>"
    exit 1
fi

if [ ! -f "$DLL_PATH" ]; then
    echo "Error: DLL not found: $DLL_PATH"
    exit 1
fi

echo "Extracting exports from $DLL_PATH..."

# Create the .def file header
cat > "$OUTPUT_DEF" << 'EOF'
; libbe.def - Export definition file for libbe.dll
; This file ensures stable ordinal numbers across rebuilds
; Generated automatically - DO NOT EDIT MANUALLY
;
; To regenerate: ./generate_libbe_def.sh builddir-wsl/src/kits/libbe.dll src/kits/libbe.def

LIBRARY libbe.dll
EXPORTS
EOF

# Extract exports with ordinals from the DLL, starting at ordinal 1
# We need to look for lines like: "        [  20] B_SOLID_HIGH"
x86_64-w64-mingw32-objdump -p "$DLL_PATH" | \
    awk '/^\s+\[\s*[0-9]+\]\s+[A-Za-z_]/ {
        # Match lines like: "        [  20] B_SOLID_HIGH"
        # Extract ordinal (inside brackets)
        match($0, /\[\s*([0-9]+)\]/, arr)
        ordinal = arr[1]
        
        # Extract symbol name (everything after the closing bracket)
        match($0, /\]\s+(.+)$/, arr)
        symbol = arr[1]
        
        # Skip lines with "Export RVA" or other non-symbol content
        if (symbol ~ /Export RVA/ || symbol ~ /\+base/ || symbol ~ /^[0-9]/) {
            next
        }
        
        # Store unique symbols with their ordinals
        # Windows DLL ordinals must start at 1 (not 0)
        if (!(symbol in seen)) {
            seen[symbol] = 1
            printf "    %s @%d\n", symbol, ordinal + 1
        }
    }' >> "$OUTPUT_DEF"

echo "Generated $OUTPUT_DEF with $(grep -c '@' "$OUTPUT_DEF") exports"
echo "Done!"
