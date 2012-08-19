var addon = require('./build/Release/sqlclient');
var EventEmitter = require('events').EventEmitter;

addon.Client.prototype.__proto__ = EventEmitter.prototype;

module.exports = addon;