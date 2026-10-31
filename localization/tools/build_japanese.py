"""Merge the Japanese source catalogs without modifying them; optionally compile."""
from pathlib import Path
import argparse
import shutil
import subprocess
import PoRW


def merged_catalog(repo):
    paths = [repo / p for p in (
        'localization/i18n/ja/OrcaSlicer_ja.po',
        'localization/flashforge/ja/orca_ja.po',
        'localization/flashforge/ja/flashforge_ja.po')]
    catalogs = []
    for path in paths:
        entries, invalid = PoRW.readMsgList(str(path), False, True)
        if invalid:
            raise ValueError('Invalid Japanese source catalog: ' + str(path))
        catalogs.append(entries)
    base, overrides, vendor = catalogs
    indexed = {PoRW.getMsgKey(e): e for e in base if e.msgId}
    for entry in overrides:
        if entry.msgId and PoRW.getMsgKey(entry) in indexed:
            indexed[PoRW.getMsgKey(entry)].lines = entry.lines
    for entry in vendor:
        if not entry.msgId:
            continue
        k = PoRW.getMsgKey(entry)
        if k in indexed:
            raise ValueError('Duplicate Japanese message: ' + k)
        indexed[k] = entry
    return '\n\n'.join(''.join(e.lines).rstrip('\r\n') for e in base + vendor) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Check that the generated catalog is up to date')
    parser.add_argument('--compile', action='store_true', help='Compile the runtime MO after merging')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    destination = repo / 'resources/i18n/ja/Orca-Flashforge_ja.po'
    text = merged_catalog(repo)
    if args.check:
        if destination.read_text(encoding='utf8') != text:
            raise SystemExit('Japanese runtime catalog is out of date; run build_japanese.py --compile')
    else:
        destination.write_text(text, encoding='utf8', newline='\n')
    if args.compile:
        bundled = repo / 'tools/msgfmt.exe'
        msgfmt = str(bundled) if bundled.exists() else shutil.which('msgfmt')
        if not msgfmt:
            raise SystemExit('GNU gettext msgfmt is required')
        subprocess.run([msgfmt, '--check-format', '--check-header', '-o',
                        str(destination.with_name('Orca-Flashforge.mo')), str(destination)], check=True)
    print('Japanese catalog is up to date.' if args.check else 'Japanese catalog generated.')


if __name__ == '__main__':
    main()
