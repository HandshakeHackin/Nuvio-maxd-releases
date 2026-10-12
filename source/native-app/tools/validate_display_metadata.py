"""Require the application metadata used by the public HDR-capable VideoOut profile.

ProsperoLight documents why attribute=0 is a functional HDR regression:
https://github.com/blackbearreloaded/ProsperoLight/blob/a6e56cf4e8e9e469d5bc5cc06ff71ad4ca16a6a9/docs/PORTING.md
attribute3=0x40040 follows ProsperoLight 01.000.095 for older firmware.
This checks the package declaration, not the console's actual HDMI capabilities.
"""
import argparse
import json
from pathlib import Path


DISPLAY_ATTRIBUTES = {'attribute': 0x62000000, 'attribute2': 0, 'attribute3': 0x40040}


def validate_display_metadata(param):
    for field, expected in DISPLAY_ATTRIBUTES.items():
        actual = param.get(field)
        if type(actual) is not int or actual != expected:
            raise ValueError(
                f'{field} must be {expected:#x} ({expected}) for the native HDR '
                f'application profile; found {actual!r}')
    if param.get('applicationCategoryType') != 0:
        raise ValueError('Nuvio requires the game application category for its direct memory budget')
    return dict(DISPLAY_ATTRIBUTES)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('param', type=Path)
    args = parser.parse_args()
    try:
        validate_display_metadata(json.loads(args.param.read_text()))
    except (ValueError, OSError) as error:
        parser.exit(1, f'Display metadata rejected: {error}\n')
    print('Display metadata: HDR-capable application profile 0x62000000; game category')


if __name__ == '__main__':
    main()
