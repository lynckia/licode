const path = require('path');

module.exports = {
  entry: './src/Erizo.js',
  output: {
    filename: 'erizo.js',
    path: path.resolve(__dirname, 'dist'),
    libraryExport: 'default',
    library: 'Erizo',
    libraryTarget: 'var',
  },
  devtool: 'source-map', // Default development sourcemap
};
