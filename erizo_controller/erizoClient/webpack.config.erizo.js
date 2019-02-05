const path = require('path');

module.exports.var = {
  entry: './src/Erizo.js',
  output: {
    filename: 'erizo.js',
    path: path.resolve(__dirname, 'dist'),
    libraryExport: 'default',
    library: 'Erizo',
    libraryTarget: 'var',
  },
  module: {
    rules: [{
      use: ['webpack-conditional-loader'],
    }],
  },
  devtool: 'source-map', // Default development sourcemap
};

module.exports.umd = {
  entry: './src/Erizo.js',
  output: {
    filename: 'erizo.js',
    path: path.resolve(__dirname, 'dist'),
    libraryExport: 'default',
    library: 'Erizo',
    libraryTarget: 'umd',
  },
  module: {
    rules: [{
      use: ['webpack-conditional-loader'],
    }],
  },
  devtool: 'source-map', // Default development sourcemap
};
