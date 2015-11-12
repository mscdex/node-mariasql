{
  'defines': [
    'HAVE_YASSL',
    'YASSL_THREAD_SAFE',
    'DBUG_OFF',
  ],
  'cflags!': [ '-O2' ],
  'cflags+': [ '-O3' ],
  'cflags_cc!': [ '-O2' ],
  'cflags_cc+': [ '-O3' ],
  'cflags_c!': [ '-O2' ],
  'cflags_c+': [ '-O3' ],
  'conditions': [
    [ 'OS=="win"', {
      # Silence compiler warnings for now. These should be fixed upstream ...
      'msvs_disabled_warnings': [4090, 4114, 4244, 4267, 4577],
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
    # TODO: This target_arch list may need to be expanded upon?
    [ 'target_arch=="arm" or target_arch=="armv7" or target_arch=="arm64"', {
      'defines': [
        'HAVE_UCONTEXT_H'
      ],
    }],
  ],
}
