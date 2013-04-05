{
  'targets': [
    {
      'target_name': 'sqlclient',
      'sources': [
        'src/binding.cc',
      ],
      'include_dirs': [
        'deps/libmariadbclient/include',
      ],
      'cflags': [ '-O3' ],
      'conditions': [
        [ 'OS=="win"', {
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
