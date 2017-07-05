const path = require('path');

module.exports = {
  entry: {
    erizo: './src/Erizo.js',
    erizofc: './src/ErizoFc.js',
  },
  output: {
    filename: '[name].js',
    path: path.resolve(__dirname, 'dist'),
    libraryExport: 'default',
    libraryTarget: 'umd',
  },
  module: {
    rules: [
      {
        test: /(.*lib.*\.js)|(.*L\..*\.js)/,
        use: ['script-loader'],
      },
    ],
  },
};
