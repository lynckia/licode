{
  'targets': [
  {
    'target_name': 'addon',
      'sources': [ 'addon.cc', 'WebRtcConnection.cc', 'OneToManyProcessor.cc', 'ExternalInput.cc', 'ExternalOutput.cc', 'OneToManyTranscoder.cc' ],
      'include_dirs' : ['$(ERIZO_HOME)/src/erizo', '$(ERIZO_HOME)/../build/libdeps/build/include'],
      'libraries': ['-L$(ERIZO_HOME)/build/erizo', '-lerizo'],
      'conditions': [
        [ 'OS=="mac"', {
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',        # -fno-exceptions
              'GCC_ENABLE_CPP_RTTI': 'YES',              # -fno-rtti
              'MACOSX_DEPLOYMENT_TARGET' : '10.7',      #from MAC OS 10.7
              'OTHER_CFLAGS': [
              '-g -O3 -stdlib=libc++',
            ]
          },
        }, { # OS!="mac"
          'cflags!' : ['-fno-exceptions'],
          'cflags' : ['-D__STDC_CONSTANT_MACROS'],
          'cflags_cc' : ['-Wall', '-O3', '-g' , '-std=c++0x', '-fexceptions'],
          'cflags_cc!' : ['-fno-exceptions'], 
          'cflags_cc!' : ['-fno-rtti']
        }],
        ]
  }
  ]
}
