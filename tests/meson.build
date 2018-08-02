test_args = [
  '-DG_LOG_DOMAIN="GnomeDesktop"',
  '-DGDK_DISABLE_DEPRECATED_WARNINGS',
  '-DGTK_DISABLE_DEPRECATED_WARNINGS',
  '-DINSTALLED_TEST_DIR="@0@"'.format(test_execdir)
]

test_env = [
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_ENABLE_DIAGNOSTICS=0'
]

test_programs = [
  'wall-clock',
 'wallclock-reftest'
]

foreach program: test_programs
  exe = executable(program, program + '.c',
    dependencies: libgnome_desktop_dep,
    c_args: test_args,
    install_dir: test_execdir,
    install: get_option('installed_tests')
  )

  test(program,
    exe,
    env: test_env
  )
endforeach

if get_option('installed_tests')
  test_data = files(
    'C.ref.ui',
    'C.ui',
    'en_US.utf-8.ref.ui',
    'en_US.utf-8.ui',
    'he_IL.utf8.ref.ui',
    'he_IL.utf8.ui'
  )
  install_data(test_data, install_dir: test_execdir)
endif
