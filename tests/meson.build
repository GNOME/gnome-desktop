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
  'bg-slide-show',
  'wall-clock',
  'wallclock-reftest',
  'languages'
]

test_metas = []

foreach program: test_programs
  exe = executable(program, program + '.c',
    dependencies: libgnome_desktop_dep,
    c_args: test_args,
    install_dir: test_execdir,
    install: get_option('installed_tests')
  )

  if get_option('installed_tests')
    meta_data = configuration_data()
    meta_data.set('TEST_EXEC', join_paths(test_execdir, program))

    test_metas += configure_file(
      configuration: meta_data,
      input: 'meta.test.in',
      output: program + '.test',
    )
  endif

  # Don't run these tests at build-time with test(): currently both tests
  # rely on our locales having been installed, and at build-time that
  # hasn't happened yet.
endforeach

if get_option('installed_tests')
  test_data = files(
    'C.ref.ui',
    'C.ui',
    'en_US.utf-8.ref.ui',
    'en_US.utf-8.ui',
    'he_IL.utf8.ref.ui',
    'he_IL.utf8.ui',
    'ja_JP.utf8.ref.ui',
    'ja_JP.utf8.ui',
    'starttime.xml'
  )
  install_data(test_data, install_dir: test_execdir)
  install_data(test_metas, install_dir: test_metadir)
endif
