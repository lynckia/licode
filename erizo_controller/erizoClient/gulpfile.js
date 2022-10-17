const gulp = require('gulp');
const log = require('fancy-log');

const plugins = {};
plugins.del = require('del');
plugins.sourcemaps = require('gulp-sourcemaps');

plugins.eslint = require('gulp-eslint');
plugins.closureCompiler = require('google-closure-compiler-js').gulp();

plugins.webpack = require('webpack');
plugins.webpackGulp = require('webpack-stream');
const errorExitCode = 2;

plugins.exitOnError = (error) => {
  log('Error running task', error);
  return process.exit(errorExitCode);
}

const config = {
  paths: {
    entry: './src/',
    debug: './dist/debug',
    production: './dist/production',
    basicExample: '../../extras/basic_example/public/',
    spine: '../../spine/',
    js: ['./src/**/*.js', '../common/semanticSdp/*.js'],
  },
};

const tasks = ['clean', 'bundle', 'compile', 'dist'];
const debugTasks = ['clean', 'bundle', 'distDebug'];
const targets = ['erizo', 'erizofc'];

const taskFunctions = {};
taskFunctions.erizo = require('./gulp/erizoTasks.js')(gulp, plugins, config);
taskFunctions.erizofc = require('./gulp/erizoFcTasks.js')(gulp, plugins, config);

const watchTasks = ['lint'];

const createTasks = (target, targetTasks, sourceTasks) => {
  sourceTasks.forEach(
    (task) => {
      const taskName = `${task}_${target}`;
      targetTasks.push(taskName);
      gulp.task(taskName, taskFunctions[target][task]);
    });
};

gulp.task('lint', () => gulp.src(config.paths.js)
  .pipe(plugins.eslint())
  .pipe(plugins.eslint.format())
  .pipe(plugins.eslint.failAfterError()));

targets.forEach(
  (target) => {
    const targetTasks = ['lint'];
    createTasks(target, targetTasks, tasks);
    createTasks(target, watchTasks, debugTasks);
    gulp.task(target, gulp.series(...targetTasks));
  });

gulp.task('watch', () => {
  const watcher = gulp.watch('src/**/*.js');
  watcher.on('change', (event) => {
    log(`File ${event.path} was ${event.type} running tasks...`);
    gulp.series(...watchTasks)();
  });
});
gulp.task('default', gulp.series(...targets));
