examples = {}

if use_qr_feature
  examples += {
    'qr-widget-example': {
      'dependencies': gnome_qr_gtk_dep,
    }
  }
endif

foreach program, options: examples
  executable(program, program + '.c',
    dependencies: options.get('dependencies', []),
    install: false,
  )
endforeach
