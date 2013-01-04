module.exports = function(grunt) {
  'use strict';
  // Project configuration.
  grunt.initConfig({
    pkg: '<json:package.json>',
    meta: {
      banner: '/*! <%= pkg.name %> - v<%= pkg.version %> - <%= grunt.template.today("yyyy-mm-dd") %> */',
      footer: 'module.exports = N;'
    },
    nodeunit: {
      files: ['test/**/*_test.js']
    },
    lint: {
       files: ['grunt.js'],
       src: ['src/N*.js']
    },
    min: {
      dist: {
        src: ['lib/xmlhttprequest.js', 'lib/hmac-sha1.js', 'src/N.js', 'src/N.Base64.js', 'src/N.API.js', 'lib/export.js'],
        dest: 'dist/nuve-client.js'
      },
      web: {
        src: ['lib/hmac-sha1.js', 'src/N.js', 'src/N.Base64.js', 'src/N.API.js'],
        dest: 'dist/nuve-web.js'
      }
    },
    watch: {
      gruntfile: {
        files: '<%= jshint.gruntfile.src %>',
        tasks: ['jshint:gruntfile']
      },
      lib: {
        files: '<%= jshint.lib.src %>',
        tasks: ['jshint:lib', 'nodeunit']
      },
      test: {
        files: '<%= jshint.test.src %>',
        tasks: ['jshint:test', 'nodeunit']
      }
    }
  });

  // Default task.
  grunt.registerTask('default', ['lint', 'min']);

};
