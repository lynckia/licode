{
  'targets': [
  {
    'target_name': 'addon',
      'sources': [ 'addon.cc', 'WebRtcConnection.cc', 'OneToManyProcessor.cc', 'ExternalInput.cc', 'ExternalOutput.cc', 'OneToManyTranscoder.cc' ],
      'include_dirs' : ['$(ERIZO_HOME)/src/erizo'],
      'libraries': ['-L$(ERIZO_HOME)/build/erizo', '-lerizo'],
      'conditions': [
        [ 'OS=="mac"', {
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',        # -fno-exceptions
              'GCC_ENABLE_CPP_RTTI': 'YES',              # -fno-rtti
              'OTHER_CFLAGS': [
              '-g -O3',
            ],
          },
        }, { # OS!="mac"
          'cflags!' : ['-fno-exceptions'],
          'cflags_cc' : ['-Wall', '-O3', '-g'],
          'cflags_cc!' : ['-fno-exceptions']

        }],
        ]
  }
  ]
}
