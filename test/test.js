var Client = require('../lib/Client');

var assert = require('assert');
var format = require('util').format;
var inspect = require('util').inspect;

var t = -1;

function NOOP(err) { assert.strictEqual(err, null); }

var tests = [
  { what: 'Non-empty threadId',
    run: function() {
      var client = makeClient();
      var threadId;
      client.connect(function() {
        threadId = client.threadId;
        client.end();
      });
      client.on('close', function() {
        assert.strictEqual(typeof threadId, 'string');
        assert.notStrictEqual(threadId.length, 0);
      });
    }
  },
  { what: 'Buffered result (defaults)',
    run: function() {
      var client = makeClient();
      var finished = false;
      client.query("SELECT 'hello' col1, 'world' col2", function(err, rows) {
        assert.strictEqual(err, null);
        assert.deepStrictEqual(
          rows,
          appendProps(
            [ {col1: 'hello', col2: 'world'} ],
            { info: {
                numRows: '1',
                affectedRows: '1',
                insertId: '0',
                metadata: undefined
              }
            }
          )
        );
        finished = true;
        client.end();
      });
      client.on('close', function() {
        assert.strictEqual(finished, true);
      });
    }
  },
  { what: 'Buffered result (metadata enabled)',
    run: function() {
      var client = makeClient();
      var finished = false;
      makeFooTable(client, {
        id: { type: 'INT', options: ['AUTO_INCREMENT', 'PRIMARY KEY'] },
        name: 'VARCHAR(255)'
      });
      client.query("INSERT INTO foo VALUES (NULL, 'hello world'),(NULL, 'bar')",
                   function(err, rows) {
        assert.strictEqual(err, null);
        assert.deepStrictEqual(
          rows,
          { info: {
              numRows: '0',
              affectedRows: '2',
              insertId: '1',
              metadata: undefined
            }
          }
        );
      });
      client.query('SELECT id, name FROM foo',
                   null,
                   { metadata: true },
                   function(err, rows) {
        assert.strictEqual(err, null);
        assert.deepStrictEqual(
          rows,
          appendProps(
            [ {id: '1', name: 'hello world'}, {id: '2', name: 'bar'} ],
            { info: {
                numRows: '2',
                affectedRows: '2',
                insertId: '1',
                metadata: {
                  id: {
                    org_name: 'id',
                    type: 'INTEGER',
                    flags: Client.NOT_NULL_FLAG
                           | Client.PRI_KEY_FLAG
                           | Client.AUTO_INCREMENT_FLAG
                           | Client.PART_KEY_FLAG
                           | Client.NUM_FLAG,
                    charsetnr: 63,
                    db: 'foo',
                    table: 'foo',
                    org_table: 'foo'
                  },
                  name: {
                    org_name: 'name',
                    type: 'VARCHAR',
                    flags: 0,
                    charsetnr: 8,
                    db: 'foo',
                    table: 'foo',
                    org_table: 'foo'
                  }
                }
              }
            }
          )
        );
        finished = true;
        client.end();
      });
      client.on('close', function() {
        assert.strictEqual(finished, true);
      });
    }
  },
];

function makeClient() {
  var client = new Client({
    host: process.env.DB_HOST || '127.0.0.1',
    port: +(process.env.DB_PORT || 3306),
    user: process.env.DB_USER || 'root',
    password: process.env.DB_PASS || ''
  });
  process.nextTick(function() {
    // Allow tests to set up `close` event handlers that gets executed first, to
    // make assertions, etc. before continuing onto next test
    client.on('close', next);
  });
  return client;
}

function appendProps(dst, src) {
  assert.strictEqual(typeof dst, 'object');
  assert.notStrictEqual(dst, null);
  assert.strictEqual(typeof src, 'object');
  assert.notStrictEqual(src, null);

  Object.keys(src).forEach(function(key) {
    dst[key] = src[key];
  });

  return dst;
}

function makeFooTable(client, colDefs, tableOpts) {
  var cols = [];
  var query;

  Object.keys(colDefs).forEach(function(name) {
    var options = [];
    var type = '';
    if (typeof colDefs[name] === 'string')
      type = colDefs[name];
    else {
      type = colDefs[name].type;
      options = colDefs[name].options;
    }
    options = options.join(' ');
    cols.push(format('`%s` %s %s', name, type, options));
  });

  if (typeof tableOpts === 'object' && tableOpts !== null) {
    var opts = [];
    Object.keys(tableOpts).forEach(function(key) {
      opts.push(format('%s=%s', key, tableOpts[key]));
    });
    tableOpts = opts.join(' ');
  } else 
    tableOpts = '';

  client.query('CREATE DATABASE IF NOT EXISTS `foo`', NOOP);
  client.query('USE `foo`', NOOP);
  query = format('CREATE TEMPORARY TABLE `foo` (%s) ENGINE=MEMORY \
                  CHARACTER SET utf8 COLLATE utf8_unicode_ci %s',
                 cols.join(', '),
                 tableOpts);
  client.query(query, NOOP);
  
}

function next() {
  if (t === tests.length - 1)
    return;
  var v = tests[++t];
  console.log("Executing '%s' ...", v.what);
  v.run.call(v);
}

function makeMsg(msg) {
  var fmtargs = ['[%s]: ' + msg, tests[t].what];
  for (var i = 1; i < arguments.length; ++i)
    fmtargs.push(arguments[i]);
  return format.apply(null, fmtargs);
}

process.once('uncaughtException', function(err) {
  if (t > -1 && !/(?:^|\n)AssertionError: /i.test(''+err))
    console.error(makeMsg('Unexpected Exception:'));
  else if (t > -1) {
    // Remake the assertion error since `JSON.stringify()` has a tendency to
    // remove properties
    err.message = format('\n%s\n%s\n%s',
                         inspect(err.actual, false, 6),
                         err.operator,
                         inspect(err.expected, false, 6));
    // Hack in the name of the test that failed
    var oldStack = err.stack;
    var oldMsg = err.message;
    err = new assert.AssertionError({
      message: makeMsg(oldMsg)
    });
    err.stack = oldStack.replace(oldMsg, err.message);
  }

  throw err;
}).once('exit', function() {
  assert(t === tests.length - 1,
         makeMsg('Only finished %d/%d tests', (t + 1), tests.length));
});

process.nextTick(next);




if (!assert.deepStrictEqual) {
  var toString = Object.prototype.toString;
  function isPrimitive(arg) {
    return arg === null || typeof arg !== 'object' && typeof arg !== 'function';
  }
  function objEquiv(a, b, strict) {
    if (a === null || a === undefined || b === null || b === undefined)
      return false;
    // if one is a primitive, the other must be same
    if (isPrimitive(a) || isPrimitive(b))
      return a === b;
    if (strict && Object.getPrototypeOf(a) !== Object.getPrototypeOf(b))
      return false;
    var aIsArgs = toString.call(a) === '[object Arguments]',
        bIsArgs = toString.call(b) === '[object Arguments]';
    if ((aIsArgs && !bIsArgs) || (!aIsArgs && bIsArgs))
      return false;
    if (aIsArgs) {
      a = pSlice.call(a);
      b = pSlice.call(b);
      return _deepEqual(a, b, strict);
    }
    var ka = Object.keys(a),
        kb = Object.keys(b),
        key, i;
    // having the same number of owned properties (keys incorporates
    // hasOwnProperty)
    if (ka.length !== kb.length)
      return false;
    //the same set of keys (although not necessarily the same order),
    ka.sort();
    kb.sort();
    //~~~cheap key test
    for (i = ka.length - 1; i >= 0; i--) {
      if (ka[i] !== kb[i])
        return false;
    }
    //equivalent values for every corresponding key, and
    //~~~possibly expensive deep test
    for (i = ka.length - 1; i >= 0; i--) {
      key = ka[i];
      if (!_deepEqual(a[key], b[key], strict)) return false;
    }
    return true;
  }
  function _deepEqual(actual, expected, strict) {
    // 7.1. All identical values are equivalent, as determined by ===.
    if (actual === expected) {
      return true;
    } else if (actual instanceof Buffer && expected instanceof Buffer) {
      return compare(actual, expected) === 0;

    // 7.2. If the expected value is a Date object, the actual value is
    // equivalent if it is also a Date object that refers to the same time.
    } else if (toString.call(actual) === '[object Date]' &&
               toString.call(expected) === '[object Date]') {
      return actual.getTime() === expected.getTime();

    // 7.3 If the expected value is a RegExp object, the actual value is
    // equivalent if it is also a RegExp object with the same source and
    // properties (`global`, `multiline`, `lastIndex`, `ignoreCase`).
    } else if (toString.call(actual) === '[object RegExp]' &&
               toString.call(expected) === '[object RegExp]') {
      return actual.source === expected.source &&
             actual.global === expected.global &&
             actual.multiline === expected.multiline &&
             actual.lastIndex === expected.lastIndex &&
             actual.ignoreCase === expected.ignoreCase;

    // 7.4. Other pairs that do not both pass typeof value == 'object',
    // equivalence is determined by ==.
    } else if ((actual === null || typeof actual !== 'object') &&
               (expected === null || typeof expected !== 'object')) {
      return strict ? actual === expected : actual == expected;

    // 7.5 For all other Object pairs, including Array objects, equivalence is
    // determined by having the same number of owned properties (as verified
    // with Object.prototype.hasOwnProperty.call), the same set of keys
    // (although not necessarily the same order), equivalent values for every
    // corresponding key, and an identical 'prototype' property. Note: this
    // accounts for both named and indexed properties on Arrays.
    } else {
      return objEquiv(actual, expected, strict);
    }
  }
  function fail(actual, expected, message, operator, stackStartFunction) {
    throw new assert.AssertionError({
      message: message,
      actual: actual,
      expected: expected,
      operator: operator,
      stackStartFunction: stackStartFunction
    });
  }
  assert.deepStrictEqual = function deepStrictEqual(actual, expected, message) {
    if (!_deepEqual(actual, expected, true)) {
      fail(actual,
           expected,
           message,
           'deepStrictEqual',
           assert.deepStrictEqual);
    }
  };
}
