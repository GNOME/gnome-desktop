project('qrcodegen', 'c')

subdir('c')

# This library contains multiple implementations in different languages,
# but only the C implementation is what we care.
# So filter them out from the dist tarball.
meson.add_dist_script('sh', '-xeu', '-c',
    'find "${MESON_PROJECT_DIST_ROOT}"/* -maxdepth 1 -type d ' +
    '-not -name c -print -exec rm -rf {} +')

# Do not include git files in the dist tarball.
meson.add_dist_script('sh', '-xeu', '-c',
    'rm -rfv "${MESON_PROJECT_DIST_ROOT}/.git"')
