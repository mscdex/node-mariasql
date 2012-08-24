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
  this.threadId = undefined;
  this.connected = false;
  this._closeOnEmpty = false;
  this._queries = [];
  this._curQuery = undefined;
  this._client = new addon.Client();
  // Client-level events
  this._client.on('connect', function() {
    self.connected = true;
    self.threadId = self._client.threadId();
    self.emit('connect');
    self._processQueries();
  });
  this._client.on('conn.error', function(err) {
    self.connected = false;
    self._reset();
    self.emit('error', err);
  });
  this._client.on('close', function(had_err) {
    self._client.connected = false;
    self.threadId = undefined;
    self._reset();
    self.emit('close', had_err);
  });
  // Query-level events
  var fnQueryErr = function(err) {
    var q = self._curQuery;
    self._curQuery = undefined;
    q.emit('error', err);
    // connection closed by server
    if (err.code === 2013 || err.code === 10001/*ERROR_HANGUP*/)
      self.emit('error', err);
    else
      self._processQueries();
  };
  this._client.on('query.error', fnQueryErr);
  this._client.on('result.error', fnQueryErr);
  this._client.on('query.abort', function() {
    var q = self._curQuery;
    self._curQuery = undefined;
    process.nextTick(function() { q.emit('abort'); });
    self._processQueries();
  });
  this._client.on('query.result', function(res) {
    self._curQuery.emit('result', res);
  });
  this._client.on('query.done', function(info) {
    var q = self._curQuery;
    self._curQuery = undefined;
    q.emit('end', info);
    self._processQueries();
  });
}
inherits(Client, EventEmitter);

Client.prototype._reset = function() {
  this._closeOnEmpty = false;
  // TODO: do not empty queries if using an auto-reconnect algorithm
  this._queries = [];
  this._curQuery = undefined;
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
  if (this._curQuery === undefined && !this._queries.length)
    this.destroy();
  else
    this._closeOnEmpty = true;
};

Client.prototype.destroy = function() {
  if (this.connected)
    this._client.end();
};

Client.prototype.query = function(query, useArray) {
  var queryObj = new Query(this, query, useArray),
      newLen = this._queries.push(queryObj);
  this._processQueries();
  return queryObj;
};

Client.prototype.escape = function(str) {
  return this._client.escape(str);
};

Client.prototype._processQueries = function() {
  if (this._curQuery === undefined && this.connected) {
    if (this._queries.length) {
      this._curQuery = this._queries.shift();
      this._client.query(this._curQuery._query, this._curQuery._useArray);
    } else if (this._closeOnEmpty)
      this.destroy();
  }
};

Client.LIB_VERSION = addon.version();

function Query(client, query, useArray) {
  this.client = client;
  this._query = query;
  this._useArray = useArray;
  this._detached = false;
}
inherits(Query, EventEmitter);
Query.prototype.abort = function() {
  if (!this._detached) {
    var i;
    if (this === this.client._curQuery) {
      // abort immediately if we're the currently running query
      this.client._client.abortQuery();
      this._detached = true;
    } else if ((i = this.client._queries.indexOf(this)) > -1) {
      // remove from the queue if the query hasn't started executing yet
      var self = this;
      this.client._queries.splice(i, 1);
      this._detached = true;
      process.nextTick(function() { self.emit('abort'); });
    }
  }
  return this._detached;
};

module.exports = Client;
