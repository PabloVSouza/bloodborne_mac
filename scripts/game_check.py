#!/usr/bin/env python3
"""The game files bbport runs: Bloodborne with the 1.09 update merged in.

Other versions start and then fail inside the game's code (the base game 1.00 faults at guest
offset 0x20348b8): hooks and patches use the addresses of this one executable, tested with
CUSA03173. The check compares the loaded executable image, whose hash is the same whatever tool
dumped it (the SELF headers around it differ), so any edition whose 1.09 executable is that one
passes. BB_SKIP_GAME_CHECK=1 skips it.
"""
import hashlib
import os
from pathlib import Path

SUPPORTED_TITLE = 'CUSA03173'
SUPPORTED_VERSION = '01.09'
SUPPORTED_IMAGE = '071df19c8880086d97182dbc057bc8cb37badaca57d9112683836b24a0444c0a'
# Bloodborne's title IDs (as in patches/Bloodborne.xml).
BLOODBORNE_TITLES = frozenset({'CUSA00207', 'CUSA00208', 'CUSA00299', 'CUSA00900', 'CUSA01363',
                               'CUSA03014', 'CUSA03023', 'CUSA03173'})
# Game of the Year editions: The Old Hunters is part of the game (others sell it as an add-on,
# and the runtime mounts no add-ons).
GOTY_TITLES = frozenset({'CUSA03023', 'CUSA03173'})
# The region from the content ID's prefix (EP9000-CUSA03173_00-...).
REGIONS = {'UP': 'america', 'EP': 'europe', 'JP': 'japan', 'HP': 'asia'}


def describe(game):
    """The dump's edition from sce_sys/param.sfo: title ID, version, region and whether The Old
    Hunters is included; None when param.sfo cannot be read."""
    from prepare import sfo
    try:
        info = sfo((Path(game) / 'sce_sys/param.sfo').read_bytes())
    except (OSError, ValueError, IndexError):
        return None
    title = info.get('TITLE_ID', '')
    return {
        'title_id': title,
        'title': info.get('TITLE', ''),
        'version': info.get('APP_VER', ''),
        'region': REGIONS.get(str(info.get('CONTENT_ID', ''))[:2], 'unknown'),
        'old_hunters': title in GOTY_TITLES,
    }


def image_sha256(game):
    """The SHA-256 of eboot.bin's loaded image (its loadable segments at their addresses)."""
    from prepare import parse_self, span  # same directory
    elf, header, ph, _segments, _missing = parse_self((Path(game) / 'eboot.bin').read_bytes())
    loads = [p for p in ph if p['type'] in (1, 0x61000010)]
    image = bytearray(max(p['vaddr'] + p['memsz'] for p in loads))
    for p in loads:
        image[p['vaddr']:p['vaddr'] + p['filesz']] = span(elf, p['offset'], p['filesz'])
    return hashlib.sha256(image).hexdigest()


def problem(game, image_hash=None):
    """None for the supported game, else (kind, title, version): kind is 'missing_update' (base
    game or an older update), 'wrong_eboot' (param.sfo says 1.09, eboot.bin is another version),
    'other_build' (another Bloodborne edition whose 1.09 executable differs), 'other_title' (not
    Bloodborne) or 'unreadable'."""
    if os.environ.get('BB_SKIP_GAME_CHECK') == '1':
        return None
    from prepare import sfo
    try:
        info = sfo((Path(game) / 'sce_sys/param.sfo').read_bytes())
    except (OSError, ValueError, IndexError):
        info = {}
    title, version = info.get('TITLE_ID', '?'), info.get('APP_VER', '?')
    try:
        if (image_hash or image_sha256(game)) == SUPPORTED_IMAGE:
            return None
    except (OSError, ValueError, IndexError, StopIteration, KeyError):
        return 'unreadable', title, version
    if title not in BLOODBORNE_TITLES:
        return 'other_title', title, version
    if version != SUPPORTED_VERSION:
        return 'missing_update', title, version
    if title != SUPPORTED_TITLE:
        return 'other_build', title, version
    return 'wrong_eboot', title, version


def explain(kind, title, version):
    """What is wrong and what to do, for the log (English)."""
    found = f'Found {title} version {version} (sce_sys/param.sfo).'
    return {
        'missing_update': f'{found} bbport needs the 1.09 update merged into the game folder: copy '
                          'everything from the dumped 1.09 update (e.g. CUSA03173-patch) into the '
                          'game folder, replacing files (eboot.bin and sce_sys too).',
        'wrong_eboot': f'{found} param.sfo is from 1.09 but eboot.bin is not: copy eboot.bin from '
                       'the dumped 1.09 update into the game folder, replacing the old one.',
        'other_build': f'{found} This edition\'s 1.09 executable is not the one bbport is built '
                       f'for (the one in {SUPPORTED_TITLE}): its hooks and patches use that '
                       'executable\'s addresses. If you dumped the update separately, merge it '
                       'into the game folder first.',
        'other_title': f'{found} This is not Bloodborne: choose the folder of your Bloodborne '
                       'dump (for example CUSA03173 or CUSA00207) with the 1.09 update merged in.',
        'unreadable': f'{found} eboot.bin could not be read as a decrypted PS4 executable: dump '
                      'the game and the 1.09 update again.',
    }[kind]
