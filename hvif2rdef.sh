#!/bin/sh

# Typical workflow when starting with an .svg file:
# 1. If your .svg has css classes, use a tool like svgo to inline the styles before converting.
#    svgo input.svg -o output.svg
# 2. Convert the inlined .svg to .hvif using icon2icon
#    icon2icon output.svg output.hvif -f hvif
# 3. Convert .hvif to .rdef using this script
#    hvif2rdef.sh output.hvif output.rdef

# Note: your .svg must have any css classes inlined, or the hvif will have incorrect colors.
# If your .svg has css classes, use a tool like svgo to inline the styles before converting.

if [ $# -ne 2 ]; then
    echo "Usage: $0 input.hvif output.rdef"
    exit 1
fi

INPUT="$1"
OUTPUT="$2"

# Extract hex bytes from HVIF
HEX=$(hexdump -v -e '1/1 "%02X"' "$INPUT")

# Write rdef header
cat > "$OUTPUT" <<EOF
resource(IconNameGoesHere) vector_icon {
EOF

# Break hex into lines (clean formatting for rdef readability)
echo "$HEX" | fold -w 64 | sed 's/^/\t$"/; s/$/"/' >> "$OUTPUT"

# Close rdef resource
echo "};" >> "$OUTPUT"

echo "Wrote $OUTPUT"