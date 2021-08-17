const path = require('path');

module.exports = {
  mode: 'production',
  entry: './src/ErizoFc.js',
  output: {
    filename: 'erizofc.js',
    path: path.resolve(__dirname, 'dist'),
    libraryExport: 'default',
    library: 'Erizo',
    libraryTarget: 'umd',
  },
};
