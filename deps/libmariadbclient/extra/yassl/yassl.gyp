{
  'targets': [
    {
      'target_name': 'yassl',
      'type': 'static_library',
      'standalone_static_library': 1,
      'includes': [ '../../config/config.gypi' ],
      'sources': [
        'src/buffer.cpp',
        'src/cert_wrapper.cpp',
        'src/crypto_wrapper.cpp',
        'src/handshake.cpp',
        'src/lock.cpp',
        'src/log.cpp',
        'src/socket_wrapper.cpp',
        'src/ssl.cpp',
        'src/timer.cpp',
        'src/yassl_error.cpp',
        'src/yassl_imp.cpp',
        'src/yassl_int.cpp',
      ],
      # Prevents node's openssl headers from getting in the way
      'include_dirs!': [ '<(node_root_dir)/include/node' ],
      'include_dirs': [
        '.',
        'include',
        'taocrypt/include',
        'taocrypt/mySTL',
        '../../include',
      ],
      'include_dirs+': [ '<(node_root_dir)/include/node' ],
    },
  ],
}
