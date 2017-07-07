const gulp = require('gulp');
const sourcemaps = require('gulp-sourcemaps');
const closureCompiler = require('google-closure-compiler-js').gulp();

const runSequence = require('run-sequence');
const del = require('del');

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
        languageIn: 'ECMASCRIPT6',
        languageOut: 'ECMASCRIPT5',
        jsOutputFile: 'erizo.js',
        createSourceMap: true,
      }))
      .pipe(sourcemaps.write('/')) // gulp-sourcemaps automatically adds the sourcemap url comment
      .pipe(gulp.dest(config.paths.distProduction));
});

gulp.task('compileErizofc', () => {
  return gulp.src(config.paths.erizofcBundle)
  .pipe(gulp.dest(config.paths.distProduction));
});

gulp.task('bundle', () => {
  return gulp.src(config.paths.entry)
  .pipe(webpackGulp(webpackConfig, webpack))
  .on('error', anError => console.log('An error ', anError))
  .pipe(gulp.dest(config.paths.distDebug))
  .on('error', anError => console.log('An error ', anError));
});

gulp.task('distBasicExample', () => {
  return gulp.src(`${config.paths.distProduction}/**/*erizo.js*`)
  .pipe(gulp.dest(config.paths.basicExampleDist));
});

gulp.task('distBasicExampleDebug', () => {
  return gulp.src(`${config.paths.distDebug}/**/*erizo.js*`)
  .pipe(gulp.dest(config.paths.basicExampleDist));
});

gulp.task('distSpine', () => {
  return gulp.src(`${config.paths.distProduction}/**/*erizofc.js*`)
  .pipe(gulp.dest(config.paths.spineDist));
});

gulp.task('dist', () => {
  runSequence('distBasicExample', 'distSpine');
});

gulp.task('watch', () => {
  const watcher = gulp.watch('src/**/*.js');
  watcher.on('change', (event) => {
    console.log('File ' + event.path + ' was ' + event.type + ', running tasks...');
    runSequence('default');
  });
});

gulp.task('default', () => {
  runSequence('lint', 'bundle', 'compileErizofc', 'compile', 'dist');
});
