#!/usr/bin/env python3
"""
sho_clean_decomp.py — Постобробка Ghidra C виводу для SLES_551.47.

Замінює нечитабельні Ghidra імена на значущі:
  - FUN_xxxxxxxx → відоме ім'я функції (з known_names.txt + class registry)
  - DAT_xxxxxxxx → відоме ім'я даних (якщо є в rodata)
  - undefined4   → int32_t
  - undefined8   → int64_t
  - undefined2   → int16_t
  - undefined1   → int8_t
  - param_1      → this_ptr (коли перший параметр — об'єктний вказівник)

Використання:
    python3 tools/sho_clean_decomp.py \\
        decomp/sho_sles_out/ghidra_full \\
        --names  docs/generated/known_names.txt \\
        --sles   game-iso/SHO/SLES_551.47 \\
        --out    decomp/sho_sles_out/ghidra_clean
"""
import argparse
import json
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
try:
    from sles import Sles
    HAS_SLES = True
except ImportError:
    HAS_SLES = False


# ─────────────────────────────────────────────────────────────────────────────
# Типи
# ─────────────────────────────────────────────────────────────────────────────

TYPE_MAP = {
    'undefined8': 'int64_t',
    'undefined4': 'int32_t',
    'undefined3': 'int32_t',  # approximation
    'undefined2': 'int16_t',
    'undefined1': 'int8_t',
    'undefined':  'uint8_t',
    'uint':       'uint32_t',
    'ushort':     'uint16_t',
    'ulong':      'uint64_t',
}

TYPE_RE = re.compile(r'\b(' + '|'.join(re.escape(k) for k in TYPE_MAP) + r')\b')


# ─────────────────────────────────────────────────────────────────────────────
# Завантаження відомих імен
# ─────────────────────────────────────────────────────────────────────────────

def load_known_names(path: str) -> dict:
    """Адреса (int) → ім'я функції. Ключ — int, без нормалізації рядка."""
    out = {}
    for raw in Path(path).read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        parts = line.split(None, 1)
        if len(parts) == 2:
            addr_str, name = parts
            try:
                addr = int(addr_str, 16)
                out[addr] = name
            except ValueError:
                pass
    return out


def load_class_registry(path: str) -> dict:
    """Додаємо factory + registry адреси з sho_class_registry.json."""
    out = {}
    data = json.loads(Path(path).read_text())
    for name, info in data.get('classes', {}).items():
        for key, label in [('factory', f'{name}_factory'),
                            ('registry', f'{name}_Register')]:
            addr_str = info.get(key)
            if addr_str:
                try:
                    out[int(addr_str, 16)] = label
                except ValueError:
                    pass
    return out


def load_attr_map(path: str) -> dict:
    """Адреса HandleAttributes → ім'я."""
    out = {}
    data = json.loads(Path(path).read_text())
    for name, info in data.items():
        if not info.get('inherited') and info.get('address'):
            try:
                out[int(info['address'], 16)] = f'{name}_HandleAttributes'
            except ValueError:
                pass
    return out


# ─────────────────────────────────────────────────────────────────────────────
# Підстановка FUN_ / DAT_ → відоме ім'я
# ─────────────────────────────────────────────────────────────────────────────

FUN_RE = re.compile(r'\bFUN_([0-9a-f]{6,8})\b', re.IGNORECASE)
DAT_RE = re.compile(r'\bDAT_([0-9a-f]{6,8})\b', re.IGNORECASE)


def make_replacer(names: dict):
    """Повертає функцію що замінює FUN_xxxxxxxx у рядку."""
    def replace_fun(m):
        addr = int(m.group(1), 16)
        known = names.get(addr)
        if known:
            # Sanitize: replace spaces and special chars with _
            safe = re.sub(r'[^A-Za-z0-9_]', '_', known)
            return safe
        return m.group(0)  # залишаємо як є

    def replace_dat(m):
        addr = int(m.group(1), 16)
        known = names.get(addr)
        if known:
            safe = re.sub(r'[^A-Za-z0-9_]', '_', known)
            return safe
        return m.group(0)

    def apply(text: str) -> str:
        text = FUN_RE.sub(replace_fun, text)
        text = DAT_RE.sub(replace_dat, text)
        return text

    return apply


# ─────────────────────────────────────────────────────────────────────────────
# Очищення одного файлу
# ─────────────────────────────────────────────────────────────────────────────

def clean_file(src: Path, dst: Path, replacer, sles=None):
    text = src.read_text(errors='replace')

    # 1. Замінити типи
    text = TYPE_RE.sub(lambda m: TYPE_MAP[m.group(1)], text)

    # 2. Замінити FUN_ / DAT_
    text = replacer(text)

    # 3. Замінити param_1 → this_ptr в методах (евристика: якщо файл
    #    зветься *_HandleAttributes або *_factory, перший параметр — об'єкт)
    fname = src.stem
    if '_HandleAttributes' in fname or '_factory' in fname:
        # Замінюємо лише у тілі функції (після першого '{')
        body_start = text.find('{')
        if body_start >= 0:
            body = text[body_start:]
            # param_1 у таких функціях завжди this
            body = re.sub(r'\bparam_1\b', 'this_obj', body)
            text = text[:body_start] + body

    # 4. Додати заголовок з нотаткою про очищення
    header_end = text.find('\n\n', text.find('*/'))
    note = '\n/* Cleaned by tools/sho_clean_decomp.py: types normalized, FUN_ resolved. */\n'
    if header_end > 0:
        text = text[:header_end] + note + text[header_end:]

    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(text)


# ─────────────────────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('indir',  help='Ghidra output dir (ghidra_full/)')
    ap.add_argument('--names',    default='docs/generated/known_names.txt')
    ap.add_argument('--registry', default='docs/generated/sho_class_registry.json')
    ap.add_argument('--attrmap',  default='docs/generated/sho_attribute_map.json')
    ap.add_argument('--sles',     default=None)
    ap.add_argument('--out', '-o', default='decomp/sho_sles_out/ghidra_clean')
    args = ap.parse_args()

    # Зібрати всі відомі імена
    names: dict = {}
    if Path(args.names).exists():
        names.update(load_known_names(args.names))
        print(f'known_names:     {len(names)} entries')
    if Path(args.registry).exists():
        extra = load_class_registry(args.registry)
        names.update(extra)
        print(f'class_registry:  +{len(extra)} entries')
    if Path(args.attrmap).exists():
        extra = load_attr_map(args.attrmap)
        names.update(extra)
        print(f'sho_attr_map:    +{len(extra)} entries')

    sles = None
    if args.sles and HAS_SLES:
        sles = Sles(args.sles)
        print(f'SLES loaded:     .text @ 0x{sles.tsa:08X}')

    replacer = make_replacer(names)

    indir  = Path(args.indir)
    outdir = Path(args.out)
    outdir.mkdir(parents=True, exist_ok=True)

    files = sorted(indir.glob('*.c'))
    print(f'\nCleaning {len(files)} files → {outdir}')

    done = 0
    resolved_total = 0
    for src in files:
        if src.name == '_index.txt':
            continue
        dst = outdir / src.name
        before = src.read_text(errors='replace')
        before_funs = len(FUN_RE.findall(before))
        clean_file(src, dst, replacer, sles)
        after = dst.read_text(errors='replace')
        after_funs = len(FUN_RE.findall(after))
        resolved = before_funs - after_funs
        resolved_total += resolved
        done += 1

    print(f'Done: {done} files, {resolved_total} FUN_ references resolved')
    print(f'Output: {outdir}/')

    # Копіюємо _index.txt якщо є
    idx = indir / '_index.txt'
    if idx.exists():
        (outdir / '_index.txt').write_text(idx.read_text())


if __name__ == '__main__':
    main()
