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
  this._closeOnEmpty = false;
  this._queries = [];
  this._curquery = undefined;
  this._client = new addon.Client();
  this._client.connected = false;
  this._client.on('connect', function() {
    self._client.connected = true;
    self.emit('connect');
    self._processQueries();
  });
  this._client.on('error', function(err) {
    if (err.when === 'query' || err.when === 'result') {
      var q = self._curquery;
      self._curquery = undefined;
      q.emit('error', err);
      // connection closed by server
      if (err.code === 2013 || err.code === 10001/*ERROR_HANGUP*/)
        self.emit('error', err);
      else
        self._processQueries();
    } else {
      self._client.connected = false;
      self._reset();
      self.emit('error', err);
    }
  });
  this._client.on('close', function(had_err) {
    self._client.connected = false;
    self._reset();
    self.emit('close', had_err);
  });
  this._client.on('queryAbort', function() {
    var q = self._curquery;
    self._curquery = undefined;
    process.nextTick(function() { q.emit('abort'); });
    self._processQueries();
  });
  this._client.on('result', function(res) {
    self._curquery.emit('result', res);
  });
  this._client.on('done', function(info) {
    var q = self._curquery;
    self._curquery = undefined;
    q.emit('end', info);
    self._processQueries();
  });
}
inherits(Client, EventEmitter);

Client.prototype._reset = function() {
  this._closeOnEmpty = false;
  // TODO: do not empty queries if using an auto-reconnect algorithm
  this._queries = [];
  this._curquery = undefined;
};

Client.prototype.isConnected = function() {
  return this._client.connected;
};

Client.prototype.isMariaDB = function() {
  return this._client.isMariaDB();
};

Client.prototype.connect = function(cfg) {
  var self = this;
  process.nextTick(function() {
    if (!isIP(cfg.host)) {
      lookup(cfg.host, function(err, address, family) {
        if (err)
          return self.emit('error', err);
        cfg = clone(cfg);
        cfg.host = address;
        self._client.connect(cfg);
      });
    } else
      self._client.connect(cfg);
  });
};

Client.prototype.end = function() {
  if (this._curquery === undefined && !this._queries.length)
    this.destroy();
  else
    this._closeOnEmpty = true;
};

Client.prototype.destroy = function() {
  this._client.end();
};

Client.prototype.query = function(query) {
  var self = this,
      queryObj = new Query(this),
      newLen = this._queries.push([query, queryObj]);
  queryObj._pairRef = this._queries[newLen - 1];
  this._processQueries();
  return queryObj;
};

Client.prototype.escape = function(str) {
  return this._client.escape(str);
};

Client.prototype._processQueries = function() {
  if (this._curquery === undefined && this.isConnected()) {
    if (this._queries.length) {
      var query = this._queries.shift();
      this._curquery = query[1];
      this._client.query(query[0]);
    } else if (this._closeOnEmpty)
      this.destroy();
  }
};

Client.LIB_VERSION = addon.version();

function Query(client) {
  this.client = client;
  this._pairRef = undefined;
  this._detached = false;
}
inherits(Query, EventEmitter);
Query.prototype.abort = function() {
  if (!this._detached) {
    var i;
    if (this === this.client.curquery) {
      this.client._client.abortQuery();
      this._detached = true;
    } else if ((i = this.client._queries.indexOf(this._pairRef)) > -1) {
      var self = this;
      this.client._queries.splice(i, 1);
      this._detached = true;
      process.nextTick(function() { self.emit('abort'); });
    }
    if (this._detached) {
      this._pairRef = undefined;
      this.client = undefined;
    }
  }
  return this._detached;
};

module.exports = Client;
