{
  'targets': [
    {
      'target_name': 'vio',
      'type': 'static_library',
      'standalone_static_library': 1,
      'includes': [ '../config/config.gypi' ],
      'sources': [
        'vio.c',
        'viosocket.c',
        'viossl.c',
        'viosslfactories.c',
      ],
      'include_dirs': [
        '.',
        '../include',
        '../extra/yassl/include',
        '../extra/yassl/taocrypt/include',
      ],
    },
  ],
}