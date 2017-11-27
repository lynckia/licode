
function Enum(...args) {
  if (!(this instanceof Enum)) {
    return new (Function.prototype.bind.apply(Enum,
      [null].concat(Array.prototype.slice.call(args))))();
  }
  Array.from(args).forEach((arg) => {
    this[arg] = Symbol.for(`LICODE_SEMANTIC_SDP_${arg}`);
  });
}

module.exports = Enum;
