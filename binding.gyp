{
  'targets': [
    {
      'target_name': 'sqlclient',
      'sources': [
        'src/binding.cc',
      ],
      'include_dirs': [
        'deps/libmariadbclient/include',
        'deps/libmariadbclient/libmysql',
        "<!(node -e \"require('nan')\")"
      ],
      'cflags': [ '-O3' ],
      'conditions': [
        [ 'OS=="win"', {
            # Re-enable warnings that were disabled for libmariadbclient
            'msvs_disabled_warnings!': [4090, 4114, 4244, 4267, 4577],
            'libraries': [
              '-lws2_32.lib',
            ],
        }],
      ],
      'includes': [ 'deps/libmariadbclient/config/config.gypi' ],
      'dependencies': [
        'deps/libmariadbclient/libmysql/clientlib.gyp:clientlib',
      ],
    },
  ],
}
