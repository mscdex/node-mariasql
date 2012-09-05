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
        'deps/libmariadbclient/zlib/zlib.gyp:zlib',
        'deps/libmariadbclient/vio/vio.gyp:vio',
        'deps/libmariadbclient/strings/strings.gyp:strings',
        'deps/libmariadbclient/extra/yassl/yassl.gyp:yassl',
        'deps/libmariadbclient/extra/yassl/taocrypt/taocrypt.gyp:taocrypt',
      ],
      'include_dirs': [
        'deps/libmariadbclient/libmysql',
        'deps/libmariadbclient/zlib',
        'deps/libmariadbclient/regex',
        'deps/libmariadbclient/sql',
        'deps/libmariadbclient/strings',
        'deps/libmariadbclient/include',
        'deps/libmariadbclient/extra/yassl/include',
        'deps/libmariadbclient/extra/yassl/taocrypt/include',
        'deps/libmariadbclient/extra/yassl/taocrypt/mySTL',
      ],
    },
  ],
}
