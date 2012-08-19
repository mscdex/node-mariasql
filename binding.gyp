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
      'cflags': [ '-O3' ],
      'conditions': [
        [ 'OS=="win"', {
          'msvs_settings': {
            'VCLinkerTool': {
              'IgnoreDefaultLibraryNames': ['libcmt.lib'],
            }
          },
          #'link_settings': {
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
          #}
        }, {
          'libraries': [
            'libmysqlclient.a',
          ],
        }],
      ]
    },
  ],
}
