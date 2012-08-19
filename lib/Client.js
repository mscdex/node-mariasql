var lookup = require('dns').lookup,
    isIP = require('net').isIP,
    inherits = require('util').inherits,
    EventEmitter = require('events').EventEmitter;

var addon = require('../build/Release/sqlclient');

addon.Client.prototype.__proto__ = EventEmitter.prototype;

function clone(o) {
  var n = Object.create(Object.getPrototypeOf(o)),
      props = Object.getOwnPropertyNames(o),
      pName, p;
  for (p in props) {
    pName = props[p];
    Object.defineProperty(n, pName, Object.getOwnPropertyDescriptor(o, pName));  
  };
  return n;
}

function Client() {
  var self = this;
  this._queries = [];
  this._curquery = undefined;
  this._client = new addon.Client();
  this._client.on('connect', function() {
    self.emit('connect');
  });
  this._client.on('error', function(err) {
    if (err.when === 'query' || err.when === 'result')
      self._curquery.emit('error', err);
    else
      self.emit('error', err);
  });
  this._client.on('close', function(had_err) {
    self.emit('close', had_err);
  });
  this._client.on('result', function(res) {
    self._curquery.emit('result', res);
  });
  this._client.on('done', function() {
    var q = self._curquery;
    self._curquery = undefined;
    q.emit('end');
    self._processQueries();
  });
}
inherits(Client, EventEmitter);

Client.prototype.connect = function(cfg) {
  var self = this;
  if (!isIP(cfg.host)) {
    lookup(cfg.host, function(err, address, family) {
      if (err)
        return self.emit('error', err);
      cfg = clone(cfg);
      cfg.host = address;
      self._client.connect(cfg);
    });
  } else
    this._client.connect(cfg);
};

Client.prototype.end = function() {
  this._client.end();
};

Client.prototype.query = function(query) {
  var self = this,
      emitter = new EventEmitter();
  this._queries.push([query, emitter]);
  this._processQueries();
  return emitter;
};

Client.prototype._processQueries = function() {
  if (this._curquery === undefined && this._queries.length) {
    var query = this._queries.shift();
    this._curquery = query[1];
    this._client.query(query[0]);
  }
};

module.exports = Client;