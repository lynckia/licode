const gulp = require('gulp');

const plugins = {};
plugins.runSequence = require('run-sequence');
plugins.del = require('del');
plugins.sourcemaps = require('gulp-sourcemaps');

plugins.eslint = require('gulp-eslint');
plugins.closureCompiler = require('google-closure-compiler-js').gulp();

plugins.webpack = require('webpack');
plugins.webpackGulp = require('webpack-stream');


const config = {
  paths: {
    entry: './src/',
    debug: './dist/debug',
    production: './dist/production',
    basicExample: '../../extras/basic_example/public/',
    spine: '../../spine/',
    js: './src/**/*.js',
  },
};

const tasks = ['clean', 'bundle', 'compile', 'dist'];
const targets = ['erizo', 'erizofc'];
const allTasks = ['lint'];


const taskFunctions = {};
taskFunctions.erizo = require('./gulp/erizoTasks.js')(gulp, plugins, config);
taskFunctions.erizofc = require('./gulp/erizoFcTasks.js')(gulp, plugins, config);

targets.forEach(
  (target) => {
    const targetTasks = ['lint'];
    tasks.forEach(
      (task) => {
        const taskName = `${task}_${target}`;
        allTasks.push(taskName);
        targetTasks.push(taskName);
        gulp.task(taskName, () => {
          return taskFunctions[target][task]()
        });
      });
    gulp.task(target, () => {
      plugins.runSequence(...targetTasks);
    })
  });

gulp.task('lint', () => {
  return gulp.src(config.paths.js)
  .pipe(plugins.eslint())
  .pipe(plugins.eslint.format())
  .pipe(plugins.eslint.failAfterError());
});

gulp.task('watch', () => {
  const watcher = gulp.watch('src/**/*.js');
  watcher.on('change', (event) => {
    console.log('File ' + event.path + ' was ' + event.type + ', running tasks...');
    plugins.runSequence('default');
  });
});

gulp.task('default', () => {
  plugins.runSequence(...allTasks);
});
