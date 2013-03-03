{
  'targets': [
    {
      'target_name': 'clientlib',
      'type': 'static_library',
      'standalone_static_library': 1,
      'includes': [ '../config/config.gypi' ],
      'sources': [
        'get_password.c',
        'libmysql.c',
        'errmsg.c',
        '../sql-common/client.c',
        '../sql-common/mysql_async.c',
        '../sql-common/my_time.c',
        '../sql-common/client_plugin.c',
        '../sql/net_serv.cc',
        '../sql-common/pack.c',
        '../sql/password.c',
      ],
      'dependencies': [
        '../mysys/mysys.gyp:mysys',
      ],
      'include_dirs': [
        '.',
        '../zlib',
        '../regex',
        '../sql',
        '../strings',
        '../include',
        '../extra/yassl/include',
        '../extra/yassl/taocrypt/include',
        '../extra/yassl/taocrypt/mySTL',
      ],
    },
  ],
}