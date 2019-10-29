/* global window */

const Random = (() => {
  const getRandomValue = () => {
    const array = new Uint16Array(1);
    window.crypto.getRandomValues(array);
    return array[0];
  };

  return {
    getRandomValue,
  };
})();

export default Random;
