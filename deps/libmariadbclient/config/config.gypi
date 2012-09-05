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
      'include_dirs': [ 'win' ],
    }, {
      'defines': [ 'HAVE_CONFIG_H', ],
    }],
    [ 'OS=="linux"', {
      'include_dirs': [ 'linux' ],
    }],
    [ 'OS=="mac"', {
      'include_dirs': [ 'mac' ],
    }],
    [ 'OS=="freebsd"', {
      'libraries': [
        '-L/usr/local/lib -lexecinfo',
      ],
      'include_dirs': [
        'freebsd',
        '/usr/local/include',
      ],
    }],
    [ 'OS=="solaris"', {
      'include_dirs': [ 'solaris' ],
    }],
  ],
}