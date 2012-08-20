
Description
===========

A [node.js](http://nodejs.org/) binding to MariaDB's non-blocking (MySQL) client
library.

This binding is different from other vanilla libmysqlclient bindings in that it
uses the non-blocking functions available in MariaDB's client library.

Therefore, this binding does **_not_** use multiple threads to achieve non-blocking
behavior.

This binding is currently tested on Windows and Linux.


Requirements
============

* [node.js](http://nodejs.org/) -- v0.8.0 or newer

* \*nix users need to install the libmariadbclient-dev package (must be 5.5.21+).
  Linux users can get this from [the MariaDB repositories](http://downloads.mariadb.org/MariaDB/repositories/).


Install
============

npm install mariasql


Examples
========

* Simple query to retrieve a list of all databases:

```javascript
var inspect = require('util').inspect;
var Client = require('mariasql');
var c = new Client();

c.on('connect', function() {
  console.log('Connected!');

  var q = c.query("SHOW DATABASES");

  q.on('result', function(result) {
    console.log('Query result: ' + inspect(result));
  });

  q.on('error', function(err) {
    console.log('Query error: ' + err);
  });

  q.on('end', function() {
    console.log('Query finished');
    c.close();
  });
});

c.on('error', function(err) {
  console.log('Client error: ' + err);
});

c.on('close', function(had_err) {
  console.log('Closed');
});

c.connect({
  host: '127.0.0.1',
  user: 'foo',
  password: 'bar'
});
```


API
===

require('mariasql') returns a **_Client_** object

Client events
-------------

* **connect**() - A connection to the server was successful.

* **error**(<_Error_>err) - An error occurred at the connection level.

* **close**(<_boolean_>hadError) - The connection was closed. `hadError` is set to true if this was due to a connection-level error.

Client methods
--------------

* **(constructor)**() - Creates and returns a new Client instance.

* **connect**(<_object_>config) - _(void)_ - Attempts a connection to a server using the information given in `config`:

    * **user** - <_string_> - Username for authentication. **Default:** (\*nix: current login name, Windows: ???)

    * **password** - <_string_> - Password for authentication. **Default:** (blank password)

    * **host** - <_string_> - Hostname or IP address of the MySQL/MariaDB server. **Default:** "localhost"

    * **port** - <_integer_> - Port number of the MySQL/MariaDB server. **Default:** 3306

    * **db** - <_string_> - A database to automatically select after authentication **Default:** (no db)

* **query**(<_string_>query) - <_EventEmitter_> - Enqueues the given `query` and returns an EventEmitter that emits the following when the query is executed:

    * **result**(<_object_>res) - `res` is an object of fieldName=>value pairs, where value is a string.

    * **end**() - No more results for this query.

    * **error**(<_Error_>err) - An error occurred while executing this query.

* **escape**(<_string_>value) - Escapes `value` for use in queries. **_This method requires a live connection_**.

* **close**(<_boolean_>immediately) - If `immediately` is true, the connection is severed even if other queries are still in the queue.


TODO
====

* Multiple statement (e.g. "SELECT * FROM foo; SELECT * FROM bar" vs. "SELECT * FROM foo") support

* Compression

* SSL encrypted connections

* Misc. query info (e.g. affected_rows, insert_id, etc)

* Auto-reconnect algorithm(s) ?

* Option to use array of field values instead of a fieldName=>value object

* Efficiently detect socket disconnection

* Possibly some other stuff I'm not aware of at the moment
