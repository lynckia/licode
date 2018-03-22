const gulp = require('gulp');

const plugins = {};
plugins.runSequence = require('run-sequence');
plugins.del = require('del');
plugins.sourcemaps = require('gulp-sourcemaps');

plugins.eslint = require('gulp-eslint');
plugins.closureCompiler = require('google-closure-compiler-js').gulp();

plugins.webpack = require('webpack');
plugins.webpackGulp = require('webpack-stream');

const errorExitCode = 2;

plugins.exitOnError = (error) => {
  console.log('Error running task', error);
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
      gulp.task(taskName, () => taskFunctions[target][task]());
    });
};

targets.forEach(
  (target) => {
    const targetTasks = ['lint'];
    createTasks(target, targetTasks, tasks);
    createTasks(target, watchTasks, debugTasks);
    gulp.task(target, () => {
      plugins.runSequence(...targetTasks);
    });
  });

gulp.task('lint', () => gulp.src(config.paths.js)
  .pipe(plugins.eslint())
  .pipe(plugins.eslint.format())
  .pipe(plugins.eslint.failAfterError()));

gulp.task('watch', () => {
  const watcher = gulp.watch('src/**/*.js');
  watcher.on('change', (event) => {
    console.log(`File ${event.path} was ${event.type} running tasks...`);
    plugins.runSequence(...watchTasks);
  });
});

gulp.task('default', () => {
  plugins.runSequence(...targets);
});
