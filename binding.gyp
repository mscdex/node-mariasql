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
      'cflags': [ '-O3', '-std=c++0x' ],
      'conditions': [
        [ 'OS=="win"', {
            # Re-enable warnings that were disabled for libmariadbclient
            # Keep C4577 disabled
            'msvs_disabled_warnings!': [4090, 4114, 4244, 4267],
            'libraries': [
              '-lws2_32.lib',
            ],
        }],
        [ 'OS=="mac"', {
          'xcode_settings': {
            'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
            'CLANG_CXX_LANGUAGE_STANDARD': 'gnu++0x',  # -std=gnu++0x
          },
        }],
      ],
      'includes': [ 'deps/libmariadbclient/config/config.gypi' ],
      'dependencies': [
        'deps/libmariadbclient/libmysql/clientlib.gyp:clientlib',
      ],
    },
  ],
}
