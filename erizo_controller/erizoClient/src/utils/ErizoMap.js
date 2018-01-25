/* global unescape */

const ErizoMap = () => {
  const that = {};
  let values = {};

  that.add = (id, value) => {
    values[id] = value;
  };

  that.get = id => values[id];

  that.has = id => values[id] !== undefined;

  that.size = () => Object.keys(values).length;

  that.forEach = (func) => {
    const keys = Object.keys(values);
    for (let index = 0; index < keys.length; index += 1) {
      const key = keys[index];
      const value = values[key];
      func(value, key);
    }
  };

  that.keys = () => Object.keys(values);

  that.remove = (id) => {
    delete values[id];
  };

  that.clear = () => {
    values = {};
  };

  return that;
};

export default ErizoMap;
