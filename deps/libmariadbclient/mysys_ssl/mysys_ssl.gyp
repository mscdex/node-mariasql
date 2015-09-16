{
  'targets': [
    {
      'target_name': 'mysys_ssl',
      'type': 'static_library',
      'standalone_static_library': 1,
      'includes': [ '../config/config.gypi' ],
      'cflags!': [ '-O3' ],
      'cflags_cc!': [ '-O3' ],
      'cflags_c!': [ '-O3' ],
      'cflags+': [ '-O2', '-g' ],
      'cflags_c+': [ '-O2', '-g' ],
      'cflags_cc+': [ '-O2' ],
      'sources': [
        'my_aes.cc',
        'my_md5.cc',
        'my_rnd.cc',
        'my_sha1.cc',
        'my_sha2.cc',
      ],
      # Prevents node's openssl headers from getting in the way
      'include_dirs!': [ '<(node_root_dir)/include/node' ],
      'include_dirs': [
        '.',
        '../include',
        '../extra/yassl/include',
        '../extra/yassl/taocrypt/include',
      ],
      'include_dirs+': [ '<(node_root_dir)/include/node' ],
    },
  ],
}

