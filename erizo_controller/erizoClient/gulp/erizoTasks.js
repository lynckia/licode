const webpackConfig = require('../webpack.config.erizo.js');

const erizoTasks = (gulp, plugins, config) => {
  const that = {};
  if (!config.paths) {
    return that;
  }
  const erizoConfig = {
    entry: `${config.paths.entry}Erizo.js`,
    webpackConfig,
    debug: `${config.paths.debug}/erizo`,
    production: `${config.paths.production}/erizo`,
  };

  that.bundle = () =>
    gulp.src(erizoConfig.entry, { base: './' })
    .pipe(plugins.webpackGulp(erizoConfig.webpackConfig, plugins.webpack))
    .on('error', anError => plugins.exitOnError(anError))
    .pipe(gulp.dest(erizoConfig.debug))
    .on('error', anError => plugins.exitOnError(anError));

  that.compile = () =>
    gulp.src(`${erizoConfig.debug}/**/*.js*`)
      .on('error', anError => plugins.exitOnError(anError))
      .pipe(gulp.dest(erizoConfig.production));

  that.dist = () =>
    gulp.src(`${erizoConfig.production}/**/*.js*`)
    .pipe(gulp.dest(config.paths.basicExample));

  that.distDebug = () =>
    gulp.src(`${erizoConfig.debug}/**/*.js*`)
    .pipe(gulp.dest(config.paths.basicExample));

  that.clean = (cb) => {
    plugins.del([`${erizoConfig.debug}/**/*.js*`, `${erizoConfig.production}/**/*.js*`],
    { force: true });
    cb();
  }

  return that;
};

module.exports = erizoTasks;
