module.exports = function(grunt) {
  'use strict';
  // Project configuration.
  grunt.initConfig({
    pkg: '<json:package.json>',
    meta: {
      banner: '/*! <%= pkg.name %> - v<%= pkg.version %> - <%= grunt.template.today("yyyy-mm-dd") %> */'
    },
    lint: {
       files: ['grunt.js'],
       src: ['src/**/*.js']
    },
    min: {
      dist: {
        src: ['lib/pre-client.js', 'src/**/*.js', 'lib/post-client.js'],
        dest: 'dist/erizo-client.js'
      },
      web: {
        src: ['lib/socket.io.js', 'src/**/*.js'],
        dest: 'dist/erizo-web.js'
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
