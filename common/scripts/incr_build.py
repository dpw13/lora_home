#!/usr/bin/env python3

import sys
import tomllib

print(f"Updating version")

VERSION_ORDER = [
        "VERSION_MAJOR",
        "VERSION_MINOR",
        "PATCHLEVEL",
        "VERSION_TWEAK",
]

with open(sys.argv[1], "rb") as ver:
        version = tomllib.load(ver)

for key in reversed(VERSION_ORDER):
        if version[key] < 255:
                version[key] += 1
                break
        else:
                version[key] = 0

with open(sys.argv[1], "w") as ver:
        for k,v in version.items():
                if isinstance(v, str):
                        ver.write(f'{k} = "{v}"\n')
                else:
                        ver.write(f'{k} = {v}\n')
        