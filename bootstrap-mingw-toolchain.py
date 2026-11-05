#!/usr/bin/env python3
"""Download a Fedora MinGW-w64 toolchain and write a Meson cross file for it.

Cosmoe's Windows build needs a full set of MinGW-w64 cross libraries (ICU,
OpenSSL, cairo/pango, freetype, fontconfig, ...). Fedora packages all of them
as mingw64-*, so this fetches those RPMs straight from the Fedora Everything
repository and unpacks them into a local prefix: no root, no dnf, no package
manager, and nothing installed outside the prefix.

The unpacked prefix has the layout dnf would produce:

    <prefix>/usr/bin/x86_64-w64-mingw32-gcc
    <prefix>/usr/x86_64-w64-mingw32/sys-root/mingw/{include,lib,bin}

The prefix doubles as a cache. Downloaded RPMs stay in <prefix>/.rpm, and an
unpacked toolchain is reused until the package list, Fedora release or mirror
changes. Delete the prefix to start over.

Usage:
    bootstrap-mingw-toolchain.py [PREFIX] [options]

Options:
    --cross-file FILE     cross file to write (default <prefix>/cross-mingw.ini)
    --fedora-release N    Fedora release to fetch from (default 44)
    --mirror URL          repository base URL (default: Fedora Everything for N)
    --package NAME        package to fetch; repeatable, replaces the defaults
    --force               ignore the cache and unpack again
    --yes                 download packages without an interactive confirmation
"""

import argparse
import json
import os
import shutil
import struct
import subprocess
import sys
import urllib.request
import xml.etree.ElementTree as ET

COMMON = {'c': 'http://linux.duke.edu/metadata/common',
          'r': 'http://linux.duke.edu/metadata/rpm'}
REPO = {'r': 'http://linux.duke.edu/metadata/repo'}

# The cross libraries Cosmoe links against, plus the compiler itself. Their
# dependencies (winpthreads, crt, headers, libstdc++, libgcc, freetype,
# harfbuzz, ...) are pulled in by resolution below.
DEFAULT_PACKAGES = [
    'mingw64-gcc',
    'mingw64-gcc-c++',
    'mingw64-binutils',
    'mingw64-cairo',
    'mingw64-pango',
    'mingw64-glib2',
    'mingw64-icu',
    'mingw64-fontconfig',
    'mingw64-libpng',
    'mingw64-zlib',
    'mingw64-libjpeg-turbo',
    'mingw64-libtiff',
    'mingw64-libwebp',
    'mingw64-openssl',
]

DEFAULT_MIRROR = 'https://dl.fedoraproject.org/pub/fedora/linux/releases/{release}/Everything/x86_64/os/'

PREFIX_SUFFIX = os.path.join('usr', 'x86_64-w64-mingw32', 'sys-root', 'mingw')

# Bumped when the generated prefix changes shape, so an existing cache is rebuilt.
STAMP_VERSION = 2
TOOLS = ('gcc', 'g++', 'ar', 'strip', 'windres', 'objdump')

# RPM payloads are compressed cpio streams; the magic decides the decompressor.
DECOMPRESSORS = (
    (b'\x28\xb5\x2f\xfd', ['zstd', '-dc']),
    (b'\xfd7zXZ\x00', ['xz', '-dc']),
    (b'\x1f\x8b', ['gzip', '-dc']),
    (b'BZh', ['bzip2', '-dc']),
    (b'070701', []),
    (b'070707', []),
)


def log(message):
    print(message, flush=True)


def fetch(url, dest, force=False):
    """Download url to dest unless it is already cached."""
    if os.path.exists(dest) and not force:
        return dest
    os.makedirs(os.path.dirname(dest) or '.', exist_ok=True)
    tmp = dest + '.part'
    for attempt in range(1, 4):
        try:
            with urllib.request.urlopen(url, timeout=120) as response, open(tmp, 'wb') as handle:
                shutil.copyfileobj(response, handle, 1 << 20)
            os.replace(tmp, dest)
            return dest
        except Exception as exc:  # network hiccup, mirror rotation, 404 on a stale index
            log('  retry %d/3 for %s (%s)' % (attempt, url, exc))
    raise SystemExit('error: could not download %s' % url)


def decompress(source, dest):
    """Decompress source to dest, guessing the format from its suffix."""
    lowered = source.lower()
    if lowered.endswith('.zst'):
        command = ['zstd', '-dc', source]
    elif lowered.endswith('.xz'):
        command = ['xz', '-dc', source]
    elif lowered.endswith('.gz'):
        command = ['gzip', '-dc', source]
    else:
        shutil.copyfile(source, dest)
        return dest
    if shutil.which(command[0]) is None:
        raise SystemExit('error: %s is required to unpack the repository metadata' % command[0])
    with open(dest, 'wb') as handle:
        subprocess.run(command, stdout=handle, check=True)
    return dest


def repodata(prefix, mirror, release, force=False):
    """Fetch repomd.xml, resolve the primary metadata and return its path."""
    cache = os.path.join(prefix, '.repodata')
    repomd = fetch(mirror + 'repodata/repomd.xml', os.path.join(cache, 'repomd.xml'), force)
    root = ET.parse(repomd).getroot()
    location = None
    for data in root.findall('r:data', REPO):
        if data.get('type') == 'primary':
            location = data.find('r:location', REPO).get('href')
            break
    if not location:
        raise SystemExit('error: %s lists no primary metadata' % repomd)
    compressed = fetch(mirror + location, os.path.join(cache, os.path.basename(location)), force)
    primary = os.path.join(cache, 'primary.xml')
    if force or not os.path.exists(primary) or os.path.getmtime(primary) < os.path.getmtime(compressed):
        decompress(compressed, primary)
    return primary, release


def index(primary):
    """Map (name, arch) to package details and capability to package."""
    byname, byprovider = {}, {}
    for _, element in ET.iterparse(primary, events=('end',)):
        if element.tag != '{%s}package' % COMMON['c']:
            continue
        name = element.find('c:name', COMMON).text
        arch = element.find('c:arch', COMMON).text
        version = element.find('c:version', COMMON)
        evr = '%s-%s' % (version.get('ver'), version.get('rel'))
        location = element.find('c:location', COMMON).get('href')
        size = int(element.find('c:size', COMMON).get('package'))
        provides = {entry.get('name') for entry
                    in element.findall('c:format/r:provides/r:entry', COMMON)}
        requires = {entry.get('name') for entry
                    in element.findall('c:format/r:requires/r:entry', COMMON)}
        key = (name, arch)
        previous = byname.get(key)
        if previous is None or evr > previous[0]:
            byname[key] = (evr, location, size, provides, requires)
        element.clear()
    for (name, arch), data in byname.items():
        for capability in data[3]:
            byprovider.setdefault(capability, (name, arch))
    return byname, byprovider


def resolver(byname, byprovider):
    def find(name):
        candidates = [key for key in byname if key[0] == name]
        if not candidates:
            return None
        candidates.sort(key=lambda key: (key[1] != 'x86_64', byname[key][0]))
        return candidates[0]

    def find_capability(capability):
        return byprovider.get(capability) or find(capability)

    return find, find_capability


def resolve(byname, byprovider, seeds):
    """Collect seeds and their mingw64 dependencies."""
    find, find_capability = resolver(byname, byprovider)
    pending, seen, plan = list(seeds), set(), []
    while pending:
        wanted = pending.pop()
        if wanted in seen:
            continue
        seen.add(wanted)
        package = find(wanted) or find_capability(wanted)
        if package is None:
            if wanted in seeds:
                raise SystemExit('error: package %s is not in the repository' % wanted)
            continue
        plan.append(package)
        for requirement in byname[package][4]:
            # Only follow Windows-target dependencies; host-only requirements
            # (redhat-rpm-config, util-linux, ...) are irrelevant here.
            if requirement.startswith('mingw64') and requirement not in seen:
                pending.append(requirement)
    return sorted(set(plan))


def header_end(handle, offset):
    """Return the offset just past the RPM header starting at offset."""
    handle.seek(offset)
    header = handle.read(16)
    if len(header) < 16 or header[:3] != b'\x8e\xad\xe8' or header[3] != 1:
        raise SystemExit('error: %s is not an RPM package' % handle.name)
    nindex, hsize = struct.unpack('>II', header[8:16])
    return offset + 16 + nindex * 16 + hsize


def extract(rpm, prefix):
    """Unpack an RPM's payload into prefix without using the rpm tool."""
    # Unbuffered: the decompressor inherits this file descriptor, and a buffered
    # reader can satisfy seek() from its read-ahead cache without an lseek,
    # which would hand the child a descriptor past the payload start.
    with open(rpm, 'rb', buffering=0) as handle:
        if handle.read(4) != b'\xed\xab\xee\xdb':
            raise SystemExit('error: %s is not an RPM package' % rpm)
        start = (header_end(handle, 96) + 7) & ~7
        start = header_end(handle, start)
        handle.seek(start)
        magic = handle.read(8)
        handle.seek(start)
        for signature, command in DECOMPRESSORS:
            if magic.startswith(signature):
                break
        else:
            raise SystemExit('error: %s uses an unsupported payload compression' % rpm)

        if shutil.which('cpio') is None:
            raise SystemExit('error: cpio is required to unpack the toolchain')
        if command:
            decoder = subprocess.Popen(command, stdin=handle, stdout=subprocess.PIPE)
            source = decoder.stdout
        else:
            decoder, source = None, handle
        cpio = subprocess.run(['cpio', '-idmu', '--quiet', '--no-absolute-filenames',
                               '-D', prefix], stdin=source)
        if source is not handle:
            source.close()
        status = decoder.wait() if decoder else 0
    if status != 0 or cpio.returncode != 0:
        raise SystemExit('error: could not unpack %s' % rpm)


def write_pkg_config_wrapper(prefix, pkgconfig, sysroot):
    """Write the pkg-config wrapper the cross file uses.

    The toolchain's .pc files contain Fedora's absolute /usr paths, so they
    have to be rebased onto the unpacked prefix. PKG_CONFIG_SYSROOT_DIR does
    exactly that, for -I and -L alike.
    """
    directory = os.path.join(prefix, 'bin')
    os.makedirs(directory, exist_ok=True)
    path = os.path.join(directory, 'mingw-pkg-config')
    script = '\n'.join([
        '#!/bin/sh',
        '# Generated by bootstrap-mingw-toolchain.py; edit that script instead.',
        "PKG_CONFIG_SYSROOT_DIR='%s'" % prefix,
        "PKG_CONFIG_LIBDIR='%s'" % os.path.join(sysroot, 'lib', 'pkgconfig'),
        "PKG_CONFIG_PATH=''",
        'export PKG_CONFIG_SYSROOT_DIR PKG_CONFIG_LIBDIR PKG_CONFIG_PATH',
        'exec \'%s\' "$@"' % pkgconfig,
        '',
    ])
    with open(path, 'w') as handle:
        handle.write(script)
    os.chmod(path, 0o755)
    return path


def write_cross_file(prefix, path):
    """Write a Meson cross file pointing at the unpacked toolchain."""
    prefix = os.path.abspath(prefix)
    binaries = os.path.join(prefix, 'usr', 'bin', 'x86_64-w64-mingw32-')
    sysroot = os.path.join(prefix, PREFIX_SUFFIX)
    pkgconfig = shutil.which('pkgconf') or shutil.which('pkg-config')
    if pkgconfig is None:
        raise SystemExit('error: pkgconf or pkg-config is required on the build machine')
    pkgconfig = write_pkg_config_wrapper(prefix, pkgconfig, sysroot)
    lines = [
        '# Generated by bootstrap-mingw-toolchain.py; edit that script or delete',
        '# %s to regenerate.' % os.path.abspath(path),
        '#',
        '# Cross file for the Fedora MinGW-w64 toolchain unpacked in %s.' % prefix,
        '# Use with: make windows-mingw',
        '',
        '[binaries]',
    ]
    for name, suffix in (('c', 'gcc'), ('cpp', 'g++'), ('ar', 'ar'),
                         ('strip', 'strip'), ('windres', 'windres')):
        lines.append("%s = '%s%s'" % (name, binaries, suffix))
    lines += [
        "pkg-config = '%s'" % pkgconfig,
        '',
        '[host_machine]',
        "system = 'windows'",
        "cpu_family = 'x86_64'",
        "cpu = 'x86_64'",
        "endian = 'little'",
        '',
        '[properties]',
        '# sys_root stays unset: the compiler has its sysroot compiled in and the',
        '# .pc files carry absolute paths. Setting it would prepend it twice.',
        "pkg_config_libdir = '%s'" % os.path.join(sysroot, 'lib', 'pkgconfig'),
        '',
    ]
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, 'w') as handle:
        handle.write('\n'.join(lines))


def main():
    parser = argparse.ArgumentParser(
        description='Download a Fedora MinGW-w64 toolchain and write a Meson cross file.')
    parser.add_argument('prefix', nargs='?', default='mingw64-toolchain',
                        help='where to unpack the toolchain (default: mingw64-toolchain)')
    parser.add_argument('--cross-file', help='cross file to write (default: <prefix>/cross-mingw.ini)')
    parser.add_argument('--fedora-release', default='44', help='Fedora release to fetch (default: 44)')
    parser.add_argument('--mirror', help='repository base URL (default: Fedora Everything)')
    parser.add_argument('--package', action='append', dest='packages',
                        help='package to fetch, repeatable; replaces the defaults')
    parser.add_argument('--force', action='store_true', help='ignore the cache')
    parser.add_argument('--yes', action='store_true',
                        help='download packages without an interactive confirmation')
    options = parser.parse_args()

    prefix = os.path.abspath(options.prefix)
    cross_file = options.cross_file or os.path.join(prefix, 'cross-mingw.ini')
    mirror = options.mirror or DEFAULT_MIRROR.format(release=options.fedora_release)
    packages = options.packages or DEFAULT_PACKAGES
    stamp = os.path.join(prefix, '.bootstrap.json')
    state = {'version': STAMP_VERSION, 'release': options.fedora_release,
             'mirror': mirror, 'packages': packages}

    compiler = os.path.join(prefix, 'usr', 'bin', 'x86_64-w64-mingw32-gcc')
    if not options.force and os.path.exists(stamp) and os.path.exists(compiler) \
            and os.path.exists(cross_file):
        with open(stamp) as handle:
            if json.load(handle) == state:
                log('MinGW-w64 toolchain already unpacked in %s' % prefix)
                log('Cross file: %s' % cross_file)
                return 0

    os.makedirs(prefix, exist_ok=True)
    log('Fetching repository metadata from %s' % mirror)
    primary, _ = repodata(prefix, mirror, options.fedora_release, options.force)
    byname, byprovider = index(primary)
    plan = resolve(byname, byprovider, packages)

    rpmdir = os.path.join(prefix, '.rpm')
    os.makedirs(rpmdir, exist_ok=True)
    total = 0
    download_size = 0
    for package in plan:
        evr, location, size, _, _ = byname[package]
        total += size
        rpm_path = os.path.join(rpmdir, os.path.basename(location))
        if not os.path.exists(rpm_path):
            download_size += size
        log('  %-34s %7.1f KiB' % ('%s (%s)' % (package[0], evr), size / 1024.0))
    log('%d packages, %.1f MiB of RPMs' % (len(plan), total / 1048576.0))

    if download_size:
        log('This Windows build setup needs to download approximately %.1f MB of MinGW-w64 packages.' %
            (download_size / 1_000_000.0))
        if not options.yes:
            try:
                answer = input('Continue? [y/N] ')
            except EOFError:
                answer = ''
            if answer.strip().lower() not in ('y', 'yes'):
                raise SystemExit('error: download cancelled (pass --yes to proceed without prompting)')

    for package in plan:
        _, location, _, _, _ = byname[package]
        rpm_path = os.path.join(rpmdir, os.path.basename(location))
        if not os.path.exists(rpm_path):
            log('Downloading %s' % os.path.basename(rpm_path))
        fetch(mirror + location, rpm_path)

    for package in plan:
        rpm = os.path.join(rpmdir, os.path.basename(byname[package][1]))
        extract(rpm, prefix)

    missing = [tool for tool in TOOLS
               if not os.path.exists(os.path.join(prefix, 'usr', 'bin', 'x86_64-w64-mingw32-' + tool))]
    if missing:
        raise SystemExit('error: toolchain is incomplete, missing %s' % ', '.join(missing))

    write_cross_file(prefix, cross_file)
    with open(stamp, 'w') as handle:
        json.dump(state, handle, indent=2)
    log('MinGW-w64 toolchain ready in %s' % prefix)
    log('Cross file: %s' % cross_file)
    return 0


if __name__ == '__main__':
    sys.exit(main())
