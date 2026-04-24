#!/usr/bin/env bash
# Generates a minimal DMG background PNG (540x380) using only built-in macOS tools.
# Output: installer/dmg_background.png
set -euo pipefail

OUT="${1:-$(dirname "$0")/dmg_background.png}"
W=540; H=380

# Use Python + built-in PIL/Pillow if available, otherwise fall back to a
# plain gradient via sips + tiffutil, or skip and let the DMG use no background.

if python3 -c "from PIL import Image, ImageDraw, ImageFont" 2>/dev/null; then
python3 - "$OUT" "$W" "$H" <<'PYEOF'
import sys
from PIL import Image, ImageDraw

out_path, W, H = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
img = Image.new("RGB", (W, H))
draw = ImageDraw.Draw(img)

# Light warm gradient background.
for y in range(H):
    t = y / H
    r = int(245 + (230 - 245) * t)
    g = int(245 + (235 - 245) * t)
    b = int(248 + (240 - 248) * t)
    draw.line([(0, y), (W, y)], fill=(r, g, b))

# Subtle bottom stripe.
for y in range(H - 48, H):
    draw.line([(0, y), (W, y)], fill=(210, 215, 225))

# Product name.
try:
    from PIL import ImageFont
    font_large = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", 28)
    font_small = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", 13)
except:
    font_large = ImageFont.load_default()
    font_small = font_large

draw.text((W // 2, 50), "Imposr", fill=(40, 50, 70), font=font_large, anchor="mm")
draw.text((W // 2, 84), "Acrobat Imposition Plug-in", fill=(100, 110, 130), font=font_small, anchor="mm")

# Arrow pointing right toward the installer icon position.
draw.text((W // 2, H - 24), "Double-click  Install Imposr  to begin", fill=(100, 110, 130), font=font_small, anchor="mm")

img.save(out_path, "PNG")
print(f"[OK] Background written to {out_path}")
PYEOF

elif command -v convert >/dev/null 2>&1; then
    # ImageMagick fallback.
    convert -size "${W}x${H}" gradient:"#F5F5F8-#E6EBF0" \
        -gravity Center -fill "#28324A" -pointsize 28 -annotate 0 "Imposr" \
        -gravity South  -fill "#646E82" -pointsize 13 \
        -annotate +0+24 "Double-click  Install Imposr  to begin" \
        "$OUT"
    echo "[OK] Background written to $OUT (ImageMagick)"
else
    echo "[WARN] No image tool found (Pillow/ImageMagick). DMG will use default background."
    exit 0
fi
