from PIL import Image
import os
import sys
import struct

RAW_DIR = "Raw"
OUT_SUBDIR = "Completed"
PALETTE_FILE = "palette.dat"  # se busca en cwd o junto al script


def _find_palette_path() -> str | None:
    # 1) en el directorio actual (donde lo lanzas)
    cwd_path = os.path.join(os.getcwd(), PALETTE_FILE)
    if os.path.isfile(cwd_path):
        return cwd_path

    # 2) junto al script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    script_path = os.path.join(script_dir, PALETTE_FILE)
    if os.path.isfile(script_path):
        return script_path

    return None


def _load_palette_768(pal_path: str) -> bytes:
    with open(pal_path, "rb") as f:
        data = f.read()
    if len(data) < 768:
        raise ValueError(
            f"'{os.path.basename(pal_path)}' es demasiado pequeño: "
            f"{len(data)} bytes (esperaba >= 768)."
        )
    return data[:768]


def _img_palette_768(img: Image.Image) -> bytes:
    pal = img.getpalette()
    if pal is None:
        return b""
    return bytes(pal[:768])


def pack_one_png(in_png: str, out_dat: str, palette_ref: bytes | None) -> None:
    img = Image.open(in_png)

    # Debe venir YA en modo paleta (Indexed)
    if img.mode != "P":
        raise ValueError(
            f"'{os.path.basename(in_png)}' no está en modo 'P' (Indexed). "
            f"Modo actual: {img.mode}."
        )

    if palette_ref is not None:
        pal_img = _img_palette_768(img)
        if pal_img != palette_ref:
            raise ValueError(
                f"'{os.path.basename(in_png)}' tiene una paleta distinta a '{PALETTE_FILE}'. "
                f"Reexporta usando la paleta global correcta."
            )

    w, h = img.size
    if w < 1 or h < 1:
        raise ValueError("Sprite inválido (tamaño 0).")

    if w > 65535 or h > 65535:
        raise ValueError("Sprite demasiado grande (máx 65535x65535).")

    pixels = img.tobytes()
    expected = w * h
    if len(pixels) != expected:
        raise RuntimeError(
            f"Datos inesperados: {len(pixels)} bytes, esperaba {expected}."
        )

    # Formato NUEVO:
    # [w:2][h:2][pixels:w*h]
    with open(out_dat, "wb") as f:
        f.write(struct.pack("<HH", w, h))  # uint16 little-endian
        f.write(pixels)


def main() -> int:
    base_dir = os.getcwd()
    raw_path = os.path.join(base_dir, RAW_DIR)
    out_path = os.path.join(raw_path, OUT_SUBDIR)

    if not os.path.isdir(raw_path):
        print(f"ERROR: no existe la carpeta '{RAW_DIR}' en {base_dir}")
        return 1

    os.makedirs(out_path, exist_ok=True)

    pal_path = _find_palette_path()
    palette_ref = None
    if pal_path:
        try:
            palette_ref = _load_palette_768(pal_path)
            print(f"Paleta: usando '{pal_path}' (primeros 768 bytes).")
        except Exception as e:
            print(f"ERROR paleta: {e}")
            return 1
    else:
        print(
            f"AVISO: no encuentro '{PALETTE_FILE}'. "
            "No se validará la paleta, solo el modo 'P'."
        )

    pngs = [fn for fn in os.listdir(raw_path) if fn.lower().endswith(".png")]
    if not pngs:
        print(f"No hay PNGs en '{RAW_DIR}'.")
        return 0

    ok = 0
    fail = 0

    for fn in sorted(pngs):
        in_png = os.path.join(raw_path, fn)
        out_dat = os.path.join(
            out_path, os.path.splitext(fn)[0] + ".dat"
        )

        try:
            pack_one_png(in_png, out_dat, palette_ref)
            ok += 1
            print(
                f"OK: {fn} -> "
                f"{os.path.join(RAW_DIR, OUT_SUBDIR, os.path.basename(out_dat))}"
            )
        except Exception as e:
            fail += 1
            print(f"FAIL: {fn} -> {e}")

    print(
        f"\nListo. OK={ok}  FAIL={fail}  "
        f"(Raw='{RAW_DIR}', salida='{RAW_DIR}/{OUT_SUBDIR}')"
    )
    return 0 if fail == 0 else 2


if __name__ == "__main__":
    sys.exit(main())
