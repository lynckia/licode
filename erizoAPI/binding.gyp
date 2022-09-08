{
  'variables' : {
    'no_debug': '0',
    'common_sources': [ 'addon.cc', 'PromiseDurationDistribution.cc', 'IOThreadPool.cc', 'AsyncPromiseWorker.cc', 'ThreadPool.cc', 'MediaStream.cc', 'WebRtcConnection.cc', 'OneToManyProcessor.cc', 'ExternalInput.cc', 'ExternalOutput.cc', 'SyntheticInput.cc', 'ConnectionDescription.cc'],
    'common_include_dirs' : ["<!(node -e \"require('nan')\")", '$(ERIZO_HOME)/src/erizo', '$(ERIZO_HOME)/../build/libdeps/build/include', '$(ERIZO_HOME)/src/third_party/webrtc/src','$(ERIZO_HOME)/src/erizo/handlers']
  },
  'targets': [
  {
    'target_name': 'addon',
      'sources': ['<@(common_sources)'],
      'include_dirs' : ['<@(common_include_dirs)', '$(ERIZO_HOME)/build/release/libdeps/log4cxx/include'],
      'libraries': ['-L$(ERIZO_HOME)/build/release/erizo -lerizo -Wl,-rpath,<(module_root_dir)/../erizo/build/release/erizo'],
      'conditions': [
        [ 'OS=="mac"', {
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',        # -fno-exceptions
            'GCC_ENABLE_CPP_RTTI': 'YES',              # -fno-rtti
            'MACOSX_DEPLOYMENT_TARGET' : '10.15',      #from MAC OS 10.7
            'OTHER_CFLAGS': ['-Qunused-arguments -Wno-deprecated -g -O3 -stdlib=libc++ -std=c++17 -DBOOST_THREAD_PROVIDES_FUTURE -DBOOST_THREAD_PROVIDES_FUTURE_CONTINUATION -DBOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY -DWEBRTC_POSIX @$(ERIZO_HOME)/conanbuildinfo.args',]
          },
        }, { # OS!="mac"
          'cflags!' : ['-fno-exceptions'],
          'cflags' : ['-D__STDC_CONSTANT_MACROS'],
          'cflags_cc' : ['-Werror', '-Wall', '-Wno-error=cast-function-type', '-Wno-error=ignored-qualifiers', '-Wno-error=attributes', '-O3', '-g', '-std=c++17', '-fexceptions', '-DBOOST_THREAD_PROVIDES_FUTURE', '-DBOOST_THREAD_PROVIDES_FUTURE_CONTINUATION', '-DBOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY', '-DWEBRTC_POSIX', '@$(ERIZO_HOME)/conanbuildinfo.args'],
          'cflags_cc!' : ['-fno-exceptions'],
          'cflags_cc!' : ['-fno-rtti']
        }],
        ]
  },
  ],
  'conditions': [
    ['no_debug!=1', {
      'targets': [
        {
          'target_name': 'addonDebug',
            'sources': ['<@(common_sources)'],
            'include_dirs' : ['<@(common_include_dirs)', '$(ERIZO_HOME)/build/debug/libdeps/log4cxx/include'],
            'libraries': ['-L$(ERIZO_HOME)/build/debug/erizo -lerizo -Wl,-rpath,<(module_root_dir)/../erizo/build/debug/erizo'],
            'conditions': [
              [ 'OS=="mac"', {
                'xcode_settings': {
                  'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',        # -fno-exceptions
                  'GCC_ENABLE_CPP_RTTI': 'YES',              # -fno-rtti
                  'MACOSX_DEPLOYMENT_TARGET' : '10.15',      #from MAC OS 10.7
                  'OTHER_CFLAGS': ['-Qunused-arguments -Wno-deprecated -g -stdlib=libc++ -std=c++17 -DBOOST_THREAD_PROVIDES_FUTURE -DBOOST_THREAD_PROVIDES_FUTURE_CONTINUATION -DBOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY -DWEBRTC_POSIX @$(ERIZO_HOME)/conanbuildinfo.args']
                },
              }, { # OS!="mac"
                'cflags!' : ['-fno-exceptions'],
                'cflags' : ['-D__STDC_CONSTANT_MACROS'],
                'cflags_cc' : ['-Werror', '-Wall', '-Wno-error=cast-function-type', '-Wno-error=ignored-qualifiers', '-Wno-error=attributes', '-g' , '-std=c++17', '-fexceptions', '-DBOOST_THREAD_PROVIDES_FUTURE', '-DBOOST_THREAD_PROVIDES_FUTURE_CONTINUATION', '-DBOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY', '-DWEBRTC_POSIX', '@$(ERIZO_HOME)/conanbuildinfo.args'],
                'cflags_cc!' : ['-fno-exceptions'],
                'cflags_cc!' : ['-fno-rtti']
              }],
              ]
        }
      ],
    }],
  ],
}
