const path = require('path');

module.exports = {
  entry: './src/ErizoFc.js',
  output: {
    filename: 'erizofc.js',
    path: path.resolve(__dirname, 'dist'),
    libraryExport: 'default',
    library: 'Erizo',
    libraryTarget: 'umd',
  },
};
