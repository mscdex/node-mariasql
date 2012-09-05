{
  'defines': [
    'HAVE_YASSL',
    'HAVE_OPENSSL',
    'YASSL_PURE_C',
    'YASSL_PREFIX',
    'YASSL_THREAD_SAFE',
    'DBUG_OFF',
  ],
  'cflags': [ '-O3' ],
  'conditions': [
    [ 'OS=="win"', {
      'conditions': [
        ['target_arch=="ia32"', {
          'include_dirs': [ 'win-x86' ],
        }, {
          'include_dirs': [ 'win-x64' ],
        }],
      ],
    }, {
      'defines': [ 'HAVE_CONFIG_H', ],
    }],
    [ 'OS=="linux"', {
      'conditions': [
        ['target_arch=="ia32"', {
          'include_dirs': [ 'linux-x86' ],
        }, {
          'include_dirs': [ 'linux-x64' ],
        }],
      ],
    }],
    [ 'OS=="mac"', {
      'conditions': [
        ['target_arch=="ia32"', {
          'include_dirs': [ 'mac-x86' ],
        }, {
          'include_dirs': [ 'mac-x64' ],
        }],
      ],
    }],
    [ 'OS=="freebsd"', {
      'libraries': [
        '-L/usr/local/lib -lexecinfo',
      ],
      'include_dirs': [
        '/usr/local/include',
      ],
      'conditions': [
        ['target_arch=="ia32"', {
          'include_dirs': [ 'freebsd-x86' ],
        }, {
          'include_dirs': [ 'freebsd-x64' ],
        }],
      ],
    }],
  ],
}