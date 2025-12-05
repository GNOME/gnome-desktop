test_args = [
  '-DG_LOG_DOMAIN="GnomeDesktop"',
  '-DGDK_DISABLE_DEPRECATED_WARNINGS',
  '-DGTK_DISABLE_DEPRECATED_WARNINGS',
  '-DINSTALLED_TEST_DIR="@0@"'.format(test_execdir)
]

test_env = [
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_ENABLE_DIAGNOSTIC=0'
]

test_programs = {}
test_metas = []

# XXX: The current batch of tests relies on the legacy library
if get_option('legacy_library')
  test_programs += {
    'bg-slide-show': {'local-test': false},
    'wall-clock': {},
    'languages': {},
  }

  test_metas += []
endif

foreach program, options: test_programs
  installed = get_option('installed_tests') and options.get('installed', true)
  test_exe = executable(program, program + '.c',
    dependencies: libgnome_desktop_dep,
    c_args: test_args,
    install_dir: test_execdir,
    install: installed
  )

  if options.get('local-test', true)
    test(program, test_exe, env: test_env)
  endif

  if installed
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
    'starttime.xml',
  )
  install_data(test_data, install_dir: test_execdir)
  install_data(test_metas, install_dir: test_metadir)
endif
