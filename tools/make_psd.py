#!/usr/bin/env python3
"""Build layered .psd files from the skin export.

    NegativeAtomiumTests --export-skin build/skin-export
    python3 tools/make_psd.py build/skin-export design/psd

Requires: pip install psd-tools
"""

import json
import sys
from pathlib import Path

from PIL import Image
from psd_tools import PSDImage
from psd_tools.api.layers import Group, PixelLayer


def build(manifest_path: Path, out_dir: Path) -> Path:
    doc = json.loads(manifest_path.read_text())
    size = (doc["width"], doc["height"])
    psd = PSDImage.new("RGB", size)
    groups = {}
    flat = Image.new("RGBA", size, (0, 0, 0, 0))

    def group_for(path: str):
        parent = psd
        key = ""
        for part in path.split("/"):
            key = f"{key}/{part}"
            if key not in groups:
                groups[key] = Group.new(parent=parent, name=part, open_folder=False)
            parent = groups[key]
        return parent

    for layer in doc["layers"]:
        image = Image.open(manifest_path.parent / layer["file"]).convert("RGBA")
        bbox = image.getbbox()
        if bbox is None:  # nothing drawn (e.g. an empty note)
            continue

        image = image.crop(bbox)
        left, top = layer["left"] + bbox[0], layer["top"] + bbox[1]
        pixel = PixelLayer.frompil(image, group_for(layer["group"]), layer["name"], top, left)
        pixel.visible = layer["visible"]

        if layer["visible"]:
            flat.alpha_composite(image, (left, top))

    reference = PixelLayer.frompil(flat, psd, "Reference (flattened, hidden)", 0, 0)
    reference.visible = False

    out_dir.mkdir(parents=True, exist_ok=True)
    out = out_dir / f"{doc['name']}.psd"
    psd.save(out)
    return out


def main() -> None:
    if len(sys.argv) != 3:
        sys.exit(__doc__)

    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    for manifest in sorted(src.glob("*.json")):
        print(build(manifest, dst))


if __name__ == "__main__":
    main()
