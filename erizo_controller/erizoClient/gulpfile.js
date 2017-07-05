const gulp = require('gulp');
const header = require('gulp-header');
const sourcemaps = require('gulp-sourcemaps');
const closureCompiler = require('google-closure-compiler').gulp();


const runSequence = require('run-sequence');
const del = require('del');
const rename = require('gulp-rename');

const eslint = require('gulp-eslint');

const webpack = require('webpack');
const webpackGulp = require('webpack-stream');
const webpackConfig = require('./webpack.config.js')

const config = {
  paths: {
    entry: './src/Erizo.js',
    baseDir: './dist',
    distDebug: './dist/debug',
    distProduction: './dist/production',
    js: './src/**/*.js',
    erizoBundle: './dist/debug/erizo.js',
    erizofcBundle: './dist/debug/erizofc.js',
    basicExampleDist: '../../extras/basic_example/public/',
    spineDist: '../../spine/',
  },
};

gulp.task('clean', () => {
  return del(['dist/**/*.js*'],
  { force: true });
});

gulp.task('lint', () => {
  return gulp.src(config.paths.js)
  .pipe(eslint());
});

gulp.task('compile', () => {
  return gulp.src(config.paths.erizoBundle, { base: './' })
      .pipe(sourcemaps.init())
      .pipe(closureCompiler({
        language_in: 'ECMASCRIPT6',
        language_out: 'ECMASCRIPT5',
        js_output_file: 'erizo.js',
      }))
      .pipe(sourcemaps.write('/')) // gulp-sourcemaps automatically adds the sourcemap url comment
      .pipe(gulp.dest(config.paths.distProduction));
});

gulp.task('bundle', () => {
  gulp.src(config.paths.entry)
  .pipe(webpackGulp(webpackConfig, webpack))
  .on('error', anError => console.log('An error ', anError))
  .pipe(gulp.dest(config.paths.distDebug))
  .on('error', anError => console.log('An error ', anError));

  return gulp.src(config.paths.erizofcBundle)
  .pipe(header('var io = require(\'socket.io-client\');'))
  .pipe(gulp.dest(config.paths.distProduction));
});

gulp.task('distBasicExample', () => {
  gulp.src(`${config.paths.distProduction}/**/*erizo.js*`)
  .pipe(gulp.dest(config.paths.basicExampleDist));
});

gulp.task('distSpine', () => {
  gulp.src(`${config.paths.distProduction}/**/*erizofc.js*`)
  .pipe(gulp.dest(config.paths.spineDist));
});

gulp.task('dist', () => {
  runSequence('distBasicExample', 'distSpine');
});

gulp.task('watch', () => {
  const watcher = gulp.watch('src/**/*.js');
  watcher.on('change', (event) => {
    console.log('File ' + event.path + ' was ' + event.type + ', running tasks...');
    runSequence('lint', 'bundle', 'compile', 'dist');
  });
});

gulp.task('default', () => {
  runSequence('lint', 'bundle', 'compile', 'dist');
});
