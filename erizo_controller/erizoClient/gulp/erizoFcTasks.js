const webpackConfig = require('../webpack.config.erizofc.js');

const erizoFcTasks = (gulp, plugins, config) => {
  const that = {};
  if (!config.paths) {
    return {};
  }
  const erizoFcConfig = {
    entry: `${config.paths.entry}ErizoFc.js`,
    webpackConfig,
    debug: `${config.paths.debug}/erizofc`,
    production: `${config.paths.production}/erizofc`,
  };
  that.bundle = () =>
    gulp.src(erizoFcConfig.entry)
    .pipe(plugins.webpackGulp(erizoFcConfig.webpackConfig, plugins.webpack))
    .on('error', anError => plugins.exitOnError(anError))
    .pipe(gulp.dest(erizoFcConfig.debug))
    .on('error', anError => plugins.exitOnError(anError));

  that.compile = () => gulp.src(`${erizoFcConfig.debug}/**/*.js`)
    .pipe(gulp.dest(erizoFcConfig.production));

  that.dist = () =>
    gulp.src(`${erizoFcConfig.production}/**/*.js`)
    .pipe(gulp.dest(config.paths.spine));

  that.distDebug = () =>
    gulp.src(`${erizoFcConfig.debug}/**/*.js`)
    .pipe(gulp.dest(config.paths.spine));

  that.clean = () =>
    plugins.del([`${erizoFcConfig.debug}/**/*.js*`, `${erizoFcConfig.production}/**/*.js*`],
    { force: true });

  return that;
};

module.exports = erizoFcTasks;
