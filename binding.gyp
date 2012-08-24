{
  'targets': [
    {
      'target_name': 'sqlclient',
      'sources': [
        'src/client.cc',
      ],
      'include_dirs': [
        'deps/libmysql-maria/include',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'cflags': [ '/O2' ],
          'msvs_settings': {
            'VCLinkerTool': {
              'IgnoreDefaultLibraryNames': ['libcmt.lib'],
            }
          },
          'conditions': [
            [ 'target_arch=="ia32"', {
              'libraries': [
                '<(PRODUCT_DIR)/../../deps/libmysql-maria/win-x86/mysqlclient.lib',
              ],
            }, {
              'libraries': [
                '<(PRODUCT_DIR)/../../deps/libmysql-maria/win-x86_64/clientlib.lib',
              ],
            }]
          ]
        }, {
          'cflags': [ '-O3' ],
          'libraries': [
            '-l:libmysqlclient.a',
          ],
        }],
      ]
    },
  ],
}
