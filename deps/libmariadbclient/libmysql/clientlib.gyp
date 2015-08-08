{
  'targets': [
    {
      'target_name': 'clientlib',
      'type': 'static_library',
      'standalone_static_library': 1,
      'includes': [ '../config/config.gypi' ],
      'sources': [
        '../sql-common/client.c',
        '../sql-common/client_plugin.c',
        '../sql-common/mysql_async.c',
        '../sql-common/my_time.c',
        '../sql-common/pack.c',
        '../sql/net_serv.cc',
        '../sql/password.c',
        'errmsg.c',
        'libmysql.c',
      ],
      'conditions': [
        [ 'OS=="win"', {
            'sources': [
              'snprintf.c',
            ],
        }],
      ],
      'dependencies': [
        '../extra/yassl/taocrypt/taocrypt.gyp:taocrypt',
        '../extra/yassl/yassl.gyp:yassl',
        '../mysys/mysys.gyp:mysys',
        '../mysys_ssl/mysys_ssl.gyp:mysys_ssl',
        '../strings/strings.gyp:strings',
        '../vio/vio.gyp:vio',
        '../zlib/zlib.gyp:zlib',
      ],
      'include_dirs': [
        '.',
        '../extra/yassl/include',
        '../extra/yassl/taocrypt/include',
        '../extra/yassl/taocrypt/mySTL',
        '../include',
        '../regex',
        '../sql',
        '../strings',
        '../zlib',
      ],
    },
  ],
}