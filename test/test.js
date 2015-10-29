var Client = require('../lib/Client');

var assert = require('assert');
var format = require('util').format;
var inspect = require('util').inspect;

var t = -1;
var testCaseTimeout = 10 * 1000;
var timeout;

var DEFAULT_HOST = process.env.DB_HOST || '127.0.0.1';
var DEFAULT_PORT = +(process.env.DB_PORT || 3306);
var DEFAULT_USER = process.env.DB_USER || 'root';
var DEFAULT_PASSWORD = process.env.DB_PASS || '';

function NOOP(err) { assert.strictEqual(err, null); }

var tests = [
  { what: 'Client::version()',
    run: function() {
      var version = Client.version();
      assert.strictEqual(typeof version, 'string');
      assert.notStrictEqual(version.length, 0);
      next();
    }
  },
  { what: 'Client::escape()',
    run: function() {
      assert.strictEqual(Client.escape("hello 'world'"), "hello \\'world\\'");
      next();
    }
  },
  { what: 'prepare()',
    run: function() {
      var client = new Client();
      var fn;
      fn = client.prepare("SELECT * FROM foo WHERE id = '123'");
      assert.strictEqual(fn({ id: 456 }),
                         "SELECT * FROM foo WHERE id = '123'");
      fn = client.prepare("SELECT * FROM foo WHERE id = :id");
      assert.strictEqual(fn({ id: 123 }),
                         "SELECT * FROM foo WHERE id = '123'");
      assert.strictEqual(fn({ id: 456 }),
                         "SELECT * FROM foo WHERE id = '456'");
      fn = client.prepare("SELECT * FROM foo WHERE id = ?");
      assert.strictEqual(fn([123]),
                         "SELECT * FROM foo WHERE id = '123'");
      assert.strictEqual(fn([456]),
                         "SELECT * FROM foo WHERE id = '456'");
      fn = client.prepare("SELECT * FROM foo WHERE id = :0 AND name = :1");
      assert.strictEqual(fn(['123', 'baz']),
                         "SELECT * FROM foo WHERE id = '123' AND name = 'baz'");

      // Edge cases
      fn = client.prepare("SELECT * FROM foo WHERE id = :id"
                          + " AND first = ? AND last = ? AND middle = '?'");
      assert.strictEqual(fn(appendProps(['foo', 'bar', 'baz'], { id: '123' })),
                         "SELECT * FROM foo WHERE id = '123'"
                         + " AND first = 'foo' AND last = 'bar'"
                         + " AND middle = '?'");
      fn = client.prepare("SELECT * FROM foo WHERE id = :id"
                          + " AND first = ? AND last = '?' AND middle = ?");
      assert.strictEqual(fn(appendProps(['foo', 'bar', 'baz'], { id: '123' })),
                         "SELECT * FROM foo WHERE id = '123'"
                         + " AND first = 'foo' AND last = '?'"
                         + " AND middle = 'bar'");
      fn = client.prepare("SELECT * FROM foo WHERE id = :id"
                          + " AND first = '?' AND last = '?' AND middle = ?");
      assert.strictEqual(fn(appendProps(['foo', 'bar', 'baz'], { id: '123' })),
                         "SELECT * FROM foo WHERE id = '123'"
                         + " AND first = '?' AND last = '?'"
                         + " AND middle = 'foo'");
      fn = client.prepare("SELECT * FROM foo WHERE id = :id"
                          + " AND first = ? AND last = '?' AND middle = '?'");
      assert.strictEqual(fn(appendProps(['foo', 'bar', 'baz'], { id: '123' })),
                         "SELECT * FROM foo WHERE id = '123'"
                         + " AND first = 'foo' AND last = '?'"
                         + " AND middle = '?'");
      next();
    }
  },
  { what: 'Non-empty threadId',
    run: function() {
      var finished = false;
      var client = makeClient(function() {
        assert.strictEqual(finished, true);
      });
      client.connect(function() {
        assert.strictEqual(typeof client.threadId, 'string');
        assert.notStrictEqual(client.threadId.length, 0);
        finished = true;
        client.end();
      });
    }
  },
  { what: 'Empty threadId (explicit disable)',
    run: function() {
      var finished = false;
      var client = makeClient({ threadId: false }, function() {
        assert.strictEqual(finished, true);
      });
      client.connect(function() {
        assert.strictEqual(client.threadId, undefined);
        finished = true;
        client.end();
      });
    }
  },
  { what: 'escape()',
    run: function() {
      var finished = false;
      var client = makeClient(function() {
        assert.strictEqual(finished, true);
      });
      client.connect(function() {
        assert.strictEqual(client.escape("hello 'world'"), "hello \\'world\\'");
        finished = true;
        client.end();
      });
    }
  },
  { what: 'isMariaDB()',
    run: function() {
      var finished = false;
      var client = makeClient(function() {
        assert.strictEqual(finished, true);
      });
      client.connect(function() {
        assert.strictEqual(typeof client.isMariaDB(), 'boolean');
        finished = true;
        client.end();
      });
    }
  },
  { what: 'serverVersion()',
    run: function() {
      var finished = false;
      var client = makeClient(function() {
        assert.strictEqual(finished, true);
      });
      client.connect(function() {
        var version = client.serverVersion();
        assert.strictEqual(typeof version, 'string');
        assert.notStrictEqual(version.length, 0);
        finished = true;
        client.end();
      });
    }
  },
  { what: 'Buffered result (defaults)',
    run: function() {
      var finished = false;
      var client = makeClient(function() {
        assert.strictEqual(finished, true);
      });
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
    }
  },
  { what: 'Buffered result (useArray)',
    run: function() {
      var finished = false;
      var client = makeClient(function() {
        assert.strictEqual(finished, true);
      });
      client.query("SELECT 'hello' col1, 'world' col2",
                   null,
                   { useArray: true },
                   function(err, rows) {
        assert.strictEqual(err, null);
        assert.deepStrictEqual(
          rows,
          appendProps(
            [ ['hello', 'world'] ],
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
    }
  },
  { what: 'Buffered result (metadata)',
    run: function() {
      var finished = false;
      var client = makeClient(function() {
        assert.strictEqual(finished, true);
      });
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
    }
  },
  { what: 'Streamed result (defaults)',
    run: function() {
      var finished = false;
      var client = makeClient(function() {
        assert.strictEqual(finished, true);
        assert.deepStrictEqual(
          events,
          [ 'result',
            [ 'result.data', {col1: 'hello', col2: 'world'} ],
            [ 'result.end',
              { numRows: '1',
                affectedRows: '-1',
                insertId: '0',
                metadata: undefined
              }
            ],
            'query.end'
          ]
        );
        finished = true;
        client.end();
      });
      var events = [];
      var query = client.query("SELECT 'hello' col1, 'world' col2");
      query.on('result', function(res) {
        events.push('result');
        res.on('data', function(row) {
          events.push(['result.data', row]);
        }).on('end', function() {
          events.push(['result.end', res.info]);
        });
      }).on('end', function() {
        events.push('query.end');
        finished = true;
        client.end();
      });
    }
  },
  { what: 'Streamed result (useArray)',
    run: function() {
      var finished = false;
      var client = makeClient(function() {
        assert.strictEqual(finished, true);
        assert.deepStrictEqual(
          events,
          [ 'result',
            [ 'result.data', ['hello', 'world'] ],
            [ 'result.end',
              { numRows: '1',
                affectedRows: '-1',
                insertId: '0',
                metadata: undefined
              }
            ],
            'query.end'
          ]
        );
        finished = true;
        client.end();
      });
      var events = [];
      var query = client.query("SELECT 'hello' col1, 'world' col2",
                               null,
                               { useArray: true });
      query.on('result', function(res) {
        events.push('result');
        res.on('data', function(row) {
          events.push(['result.data', row]);
        }).on('end', function() {
          events.push(['result.end', res.info]);
        });
      }).on('end', function() {
        events.push('query.end');
        finished = true;
        client.end();
      });
    }
  },
  { what: 'Streamed result (metadata)',
    run: function() {
      var finished = false;
      var client = makeClient(function() {
        assert.strictEqual(finished, true);
        assert.deepStrictEqual(
          events,
          [ 'result',
            [ 'result.data', {id: '1', name: 'hello world'} ],
            [ 'result.data', {id: '2', name: 'bar'} ],
            [ 'result.end',
              { numRows: '2',
                affectedRows: '-1',
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
            ],
            'query.end'
          ]
        );
        finished = true;
        client.end();
      });
      var events = [];
      makeFooTable(client, {
        id: { type: 'INT', options: ['AUTO_INCREMENT', 'PRIMARY KEY'] },
        name: 'VARCHAR(255)'
      });
      client.query("INSERT INTO foo VALUES (NULL, 'hello world'),(NULL, 'bar')",
                   NOOP);
      var query = client.query('SELECT id, name FROM foo',
                               null,
                               { metadata: true });
      query.on('result', function(res) {
        events.push('result');
        res.on('data', function(row) {
          events.push(['result.data', row]);
        }).on('end', function() {
          events.push(['result.end', res.info]);
        });
      }).on('end', function() {
        events.push('query.end');
        finished = true;
        client.end();
      });
    }
  },
  { what: 'Streamed result (INSERT)',
    run: function() {
      var finished = false;
      var client = makeClient(function() {
        assert.strictEqual(finished, true);
        assert.deepStrictEqual(
          events,
          [ 'result',
            [ 'result.end',
              { numRows: '0',
                affectedRows: '2',
                insertId: '1',
                metadata: undefined
              }
            ],
            'query.end'
          ]
        );
        finished = true;
        client.end();
      });
      var events = [];
      makeFooTable(client, {
        id: { type: 'INT', options: ['AUTO_INCREMENT', 'PRIMARY KEY'] },
        name: 'VARCHAR(255)'
      });
      var query = client.query(
        "INSERT INTO foo VALUES (NULL, 'hello world'),(NULL, 'bar')"
      );
      query.on('result', function(res) {
        events.push('result');
        res.on('data', function(row) {
          events.push(['result.data', row]);
        }).on('end', function() {
          events.push(['result.end', res.info]);
        });
      }).on('end', function() {
        events.push('query.end');
        finished = true;
        client.end();
      });
    }
  },
  { what: 'Streamed result (START TRANSACTION, no data listener)',
    run: function() {
      var finished = false;
      var client = makeClient(function() {
        assert.strictEqual(finished, true);
        assert.deepStrictEqual(
          events,
          [ 'result',
            [ 'result.end',
              { numRows: '0',
                affectedRows: '0',
                insertId: '0',
                metadata: undefined
              }
            ],
            'query.end'
          ]
        );
        finished = true;
        client.end();
      });
      var events = [];
      var query = client.query('START TRANSACTION');
      query.on('result', function(res) {
        events.push('result');
        res.on('end', function() {
          events.push(['result.end', res.info]);
        });
      }).on('end', function() {
        events.push('query.end');
        finished = true;
        client.end();
      });
    }
  },
  { what: 'lastInsertId()',
    run: function() {
      var finished = false;
      var client = makeClient(function() {
        assert.strictEqual(finished, true);
      });
      makeFooTable(client, {
        id: { type: 'INT', options: ['AUTO_INCREMENT', 'PRIMARY KEY'] },
        name: 'VARCHAR(255)'
      });
      client.query("INSERT INTO foo (id, name) VALUES (NULL, 'hello')",
                   function(err) {
        assert.strictEqual(err, null);
        assert.strictEqual(client.lastInsertId(), '1');
        client.query("INSERT INTO foo (id, name) VALUES (NULL, 'world')",
                     function(err) {
          assert.strictEqual(err, null);
          assert.strictEqual(client.lastInsertId(), '2');
          finished = true;
          client.end();
        });
      });
    }
  },
  { what: 'multiStatements',
    run: function() {
      var finished = false;
      var client = makeClient({ multiStatements: true }, function() {
        assert.strictEqual(finished, true);
      });
      client.query("SELECT 'hello' c1; SELECT 'world' c2", function(err, rows) {
        assert.strictEqual(err, null);
        assert.deepStrictEqual(
          rows,
          [ appendProps(
              [ { c1: 'hello' } ],
              { info: {
                  numRows: '1',
                  affectedRows: '1',
                  insertId: '0',
                  metadata: undefined
                }
              }
            ),
            appendProps(
              [ { c2: 'world' } ],
              { info: {
                  numRows: '1',
                  affectedRows: '1',
                  insertId: '0',
                  metadata: undefined
                }
              }
            )
          ]
        );
        finished = true;
        client.end();
      });
    }
  },
  { what: 'Process queue before connection close',
    run: function() {
      var finished = false;
      var client = makeClient(function() {
        assert.strictEqual(finished, true);
      });
      client.query("SELECT 'hello' col1", function(err, rows) {
        assert.strictEqual(err, null);
        assert.deepStrictEqual(
          rows,
          appendProps(
            [ {col1: 'hello'} ],
            { info: {
                numRows: '1',
                affectedRows: '1',
                insertId: '0',
                metadata: undefined
              }
            }
          )
        );
      });
      client.query("SELECT 'world' col2", function(err, rows) {
        assert.strictEqual(err, null);
        assert.deepStrictEqual(
          rows,
          appendProps(
            [ {col2: 'world'} ],
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
      });
      client.end();
    }
  },
  { what: 'Abort long running query',
    run: function() {
      var finished = false;
      var sawAbortCb = false;
      var closes = 0;
      var client = makeClient({ _skipClose: true });
      client.on('close', function() {
        assert.strictEqual(++closes, 1);
        checkDone();
      });
      client.query("SELECT SLEEP(60) ret", function(err, rows) {
        assert.strictEqual(err, null);
        assert.deepStrictEqual(
          rows,
          appendProps(
            [ {ret: '1'} ],
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
        checkDone();
      });
      setTimeout(function() {
        client.abort(function(err) {
          assert.strictEqual(err, null);
          sawAbortCb = true;
          checkDone();
        });
      }, 1000);

      function checkDone() {
        if (finished && sawAbortCb && closes === 1)
          next();
      }
    }
  },
  { what: 'Abort connection',
    run: function() {
      var finished = false;
      var sawAbortCb = false;
      var sawError = false;
      var closes = 0;
      var client = makeClient({ _skipClose: true });
      client.on('close', function() {
        assert.strictEqual(++closes, 1);
        checkDone();
      });
      client.on('error', function(err) {
        assert.strictEqual(sawError, false);
        assert.strictEqual(typeof err, 'object');
        assert.strictEqual(err.code, 2013);
        assert.strictEqual(client.connected, false);
        sawError = true;
      });
      client.query("SELECT SLEEP(60) ret", function(err, rows) {
        assert.strictEqual(typeof err, 'object');
        assert.strictEqual(err.code, 2013);
        assert.strictEqual(rows, undefined);
        assert.strictEqual(client.connected, false);
        finished = true;
        checkDone();
      });
      setTimeout(function() {
        client.abort(true, function(err) {
          assert.strictEqual(err, null);
          sawAbortCb = true;
          checkDone();
        });
      }, 1000);

      function checkDone() {
        if (finished && sawAbortCb && sawError && closes === 1)
          next();
      }
    }
  },
  { what: 'clean up pending queries on close',
    run: function() {
      var finished1 = false;
      var finished2 = false;
      var finished3 = false;
      var sawAbortCb = false;
      var sawError = false;
      var closes = 0;
      var client = makeClient({ _skipClose: true });
      client.on('close', function() {
        assert.strictEqual(++closes, 1);
        checkDone();
      });
      client.on('error', function(err) {
        assert.strictEqual(sawError, false);
        assert.strictEqual(typeof err, 'object');
        assert.strictEqual(err.code, 2013);
        assert.strictEqual(client.connected, false);
        sawError = true;
      });
      client.query("SELECT SLEEP(60) ret", function(err, rows) {
        assert.strictEqual(typeof err, 'object');
        assert.strictEqual(err.code, 2013);
        assert.strictEqual(rows, undefined);
        assert.strictEqual(client.connected, false);
        finished1 = true;
      });
      client.query("SELECT SLEEP(0.1) ret", function(err, rows) {
        assert.strictEqual(typeof err, 'object');
        assert.strictEqual(err.code, 2013);
        assert.strictEqual(rows, undefined);
        assert.strictEqual(client.connected, false);
        finished2 = true;
      });
      client.query("SELECT SLEEP(0.1) ret", function(err, rows) {
        assert.strictEqual(typeof err, 'object');
        assert.strictEqual(err.code, 2013);
        assert.strictEqual(rows, undefined);
        assert.strictEqual(client.connected, false);
        finished3 = true;
        checkDone();
      });
      setTimeout(function() {
        client.abort(true, function(err) {
          assert.strictEqual(err, null);
          sawAbortCb = true;
          checkDone();
        });
      }, 1000);

      function checkDone() {
        if (finished1 && finished2 && finished3 && sawAbortCb && sawError
            && closes === 1) {
          next();
        }
      }
    }
  },
  { what: 'keep pending queries on close',
    run: function() {
      var finished1 = false;
      var finished2 = false;
      var finished3 = false;
      var sawAbortCb = false;
      var sawError = false;
      var closes = 0;
      var client = makeClient({ keepQueries: true, _skipClose: true });
      client.on('close', function() {
        assert.strictEqual(finished1, true);
        assert.strictEqual(sawError, true);
        if (++closes === 1) {
          assert.strictEqual(finished2, false);
          assert.strictEqual(finished3, false);
          client.connect();
          return;
        }
        assert.strictEqual(closes, 2);
        assert.strictEqual(finished2, true);
        assert.strictEqual(finished3, true);
        checkDone();
      });
      client.on('error', function(err) {
        assert.strictEqual(sawError, false);
        assert.strictEqual(typeof err, 'object');
        assert.strictEqual(err.code, 2013);
        assert.strictEqual(client.connected, false);
        sawError = true;
      });
      client.query("SELECT SLEEP(60) ret", function(err, rows) {
        assert.strictEqual(typeof err, 'object');
        assert.strictEqual(err.code, 2013);
        assert.strictEqual(rows, undefined);
        assert.strictEqual(client.connected, false);
        finished1 = true;
      });
      client.query("SELECT SLEEP(0.1) ret", function(err, rows) {
        assert.strictEqual(err, null);
        assert.deepStrictEqual(
          rows,
          appendProps(
            [ {ret: '0'} ],
            { info: {
                numRows: '1',
                affectedRows: '1',
                insertId: '0',
                metadata: undefined
              }
            }
          )
        );
        assert.strictEqual(client.connected, true);
        finished2 = true;
      });
      client.query("SELECT SLEEP(0.1) ret", function(err, rows) {
        assert.strictEqual(err, null);
        assert.deepStrictEqual(
          rows,
          appendProps(
            [ {ret: '0'} ],
            { info: {
                numRows: '1',
                affectedRows: '1',
                insertId: '0',
                metadata: undefined
              }
            }
          )
        );
        assert.strictEqual(client.connected, true);
        finished3 = true;
        client.end();
      });
      setTimeout(function() {
        client.abort(true, function(err) {
          assert.strictEqual(err, null);
          sawAbortCb = true;
          checkDone();
        });
      }, 1000);

      function checkDone() {
        if (finished1 && finished2 && finished3 && sawAbortCb && sawError
            && closes === 2) {
          next();
        }
      }
    }
  },
  { what: 'No server running',
    run: function() {
      var client = makeClient({ port: 3005 }, function() {
        assert.deepStrictEqual(
          events,
          [ 'client.error' ]
        );
        client.end();
      });
      var events = [];
      var query = client.query('SELECT 1');
      query.on('result', function(res) {
        events.push('result');
        res.on('data', function(row) {
          events.push(['result.data']);
        }).on('end', function() {
          events.push(['result.end']);
        });
      }).on('end', function() {
        events.push('query.end');
      });

      client.on('error', function(err) {
        assert.strictEqual(err.code, 2003);
        events.push('client.error');
      }).on('ready', function() {
        events.push('ready');
      });
    }
  },
  { what: 'Bad auth',
    run: function() {
      var client = makeClient({ password: 'foobarbaz890' }, function() {
        assert.deepStrictEqual(
          events,
          [ 'client.error' ]
        );
        client.end();
      });
      var events = [];
      var query = client.query('SELECT 1');
      query.on('result', function(res) {
        events.push('result');
        res.on('data', function(row) {
          events.push(['result.data']);
        }).on('end', function() {
          events.push(['result.end']);
        });
      }).on('end', function() {
        events.push('query.end');
      });

      client.on('error', function(err) {
        assert.strictEqual(err.code, 1045);
        events.push('client.error');
      }).on('ready', function() {
        events.push('ready');
      });
    }
  },
  { what: 'Stored procedure',
    run: function() {
      var client = makeClient();
      client.query('CREATE DATABASE IF NOT EXISTS `foo`', NOOP);
      client.query('USE `foo`', NOOP);
      client.query('DROP PROCEDURE IF EXISTS `testproc`', NOOP);
      client.query('CREATE PROCEDURE `testproc` ()\
                    BEGIN\
                      SELECT 1;\
                    END');
      client.query('CALL testproc', function(err, rows) {
        assert.strictEqual(err, null);
        assert.deepStrictEqual(
          rows,
          [
            appendProps(
              [ {1: '1'} ],
              { info: {
                  numRows: '1',
                  affectedRows: '1',
                  insertId: '0',
                  metadata: undefined
                }
              }
            ),
            { info: {
                numRows: '0',
                affectedRows: '0',
                insertId: '0',
                metadata: undefined
              }
            }
          ]
        );
        assert.strictEqual(client.connected, true);
        client.end();
      });
      client.query('DROP PROCEDURE IF EXISTS `testproc`', NOOP);
    }
  },
  { what: 'Stored procedure (bad second query)',
    run: function() {
      var client = makeClient();
      client.query('CREATE DATABASE IF NOT EXISTS `foo`', NOOP);
      client.query('USE `foo`', NOOP);
      client.query('DROP PROCEDURE IF EXISTS `testproc`', NOOP);
      client.query('CREATE PROCEDURE `testproc` ()\
                    BEGIN\
                      SELECT 1;\
                      SELECT f;\
                    END');
      client.query('CALL testproc', function(err, rows) {
        assert.strictEqual(err, null);
        assert.strictEqual(rows.length, 2);
        assert.deepStrictEqual(
          rows[0],
          appendProps(
            [ {1: '1'} ],
            { info: {
                numRows: '1',
                affectedRows: '1',
                insertId: '0',
                metadata: undefined
              }
            }
          )
        );
        assert.strictEqual(rows[1].code, 1054);
        assert.strictEqual(client.connected, true);
        client.end();
      });
      client.query('DROP PROCEDURE IF EXISTS `testproc`', NOOP);
    }
  },
];

function makeClient(opts, closeCb) {
  var config = {
    host: DEFAULT_HOST,
    port: DEFAULT_PORT,
    user: DEFAULT_USER,
    password: DEFAULT_PASSWORD
  };

  if (typeof opts === 'object' && opts !== null) {
    Object.keys(opts).forEach(function(key) {
      if (key !== '_skipClose') // Internal option for testing only
        config[key] = opts[key];
    });
  } else if (typeof opts === 'function') {
    closeCb = opts;
    opts = null;
  }

  var client = new Client(config);

  if (!opts || !opts._skipClose) {
    process.nextTick(function() {
      if (typeof closeCb === 'function')
        client.on('close', closeCb);
      client.on('close', next);
    });
  }

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
  clearTimeout(timeout);
  if (t > -1)
    console.log('Finished %j', tests[t].what)
  if (t === tests.length - 1)
    return;
  var v = tests[++t];
  timeout = setTimeout(function() {
    throw new Error(format('Test case %j timed out', v.what));
  }, testCaseTimeout);
  console.log('Executing %j', v.what);
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
    // Only change the message format when it's necessary to make it potentially
    // more readable
    if ((typeof err.actual === 'object' && err.actual !== null)
        || (typeof err.expected === 'object' && err.expected !== null)) {
      // Remake the assertion error since `JSON.stringify()` has a tendency to
      // remove properties from `.actual` and `.expected` objects
      err.message = format('\n%s\n%s\n%s',
                           inspect(err.actual, false, 6),
                           err.operator,
                           inspect(err.expected, false, 6));
    }

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
