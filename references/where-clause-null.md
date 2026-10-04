<!-- Source: https://use-the-index-luke.com/sql/where-clause/null — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# NULL in the Oracle Database

<sub>Source: https://use-the-index-luke.com/sql/where-clause/null</sub>

SQL’s `NULL` frequently causes confusion. Although the basic idea of `NULL`—[to represent missing data](https://en.wikipedia.org/wiki/Null_%28SQL%29)—is rather simple, there are some peculiarities. You have to use `IS NULL` instead of `= NULL`, for example. Moreover the Oracle database has additional `NULL` oddities, on the one hand because it does not always handle `NULL` as required by the standard and on the other hand because it has a very “special” handling of `NULL` in indexes.

The SQL standard does not define `NULL` as a value but rather as a placeholder for a missing or unknown value. Consequently, no value can be `NULL`. Instead the Oracle database treats an empty string as `NULL`:

```
   SELECT     '0 IS NULL???' AS "what is NULL?" FROM dual
    WHERE      0 IS NULL
UNION ALL
   SELECT    '0 is not null' FROM dual
    WHERE     0 IS NOT NULL
UNION ALL
   SELECT ''''' IS NULL???'  FROM dual
    WHERE    '' IS NULL
UNION ALL
   SELECT ''''' is not null' FROM dual
    WHERE    '' IS NOT NULL
```

To add to the confusion, there is even a case when the Oracle database treats `NULL` as empty string:

```
SELECT dummy
     , dummy || ''
     , dummy || NULL
  FROM dual
```

Concatenating the `DUMMY` column (always containing `'X'`) with `NULL` should return `NULL`.

The concept of `NULL` is used in many programming languages. No matter where you look, an empty string is never `NULL`…except in the Oracle database. It is, in fact, impossible to store an empty string in a `VARCHAR2` field. If you try, the Oracle database just stores `NULL`.

This peculiarity is not only strange; it is also dangerous. Additionally the Oracle database’s `NULL` oddity does not stop here—it continues with indexing.

## Contents

1. *[`NULL` in Indexes](#indexing-null)* — Every index is a partial index
2. *[`NOT NULL` Constraints](#not-null-constraints)* — affect index usage
3. *[Emulating Partial Indexes](#emulating-partial-indexes-in-the-oracle-database)* — using function-based indexing


## Indexing NULL

<sub>Source: https://use-the-index-luke.com/sql/where-clause/null/index</sub>

The Oracle database does not include rows in an index if all indexed columns are `NULL`. That means that every index is a [partial index](where-clause-partial-and-filtered-indexes.md)—like having a `where` clause:

```
CREATE INDEX idx
          ON tbl (A, B, C, ...)
       WHERE A IS NOT NULL
          OR B IS NOT NULL
          OR C IS NOT NULL
             ...
```

Consider the `EMP_DOB` index. It has only one column: the `DATE_OF_BIRTH`. A row that does not have a `DATE_OF_BIRTH` value is not added to this index.

```
INSERT INTO employees ( subsidiary_id, employee_id
                      , first_name   , last_name
                      , phone_number)
               VALUES ( ?, ?, ?, ?, ? )
```

The `insert` statement does not set the `DATE_OF_BIRTH` so it defaults to `NULL`—hence, the record is not added to the `EMP_DOB` index. As a consequence, the index cannot support a query for records where `DATE_OF_BIRTH` `IS NULL`:

```
SELECT first_name, last_name
  FROM employees
 WHERE date_of_birth IS NULL
```

Nevertheless, the record is inserted into a concatenated index if at least one index column is not `NULL`:

```
CREATE INDEX demo_null
          ON employees (subsidiary_id, date_of_birth)
```

The above created row is added to the index because the `SUBSIDIARY_ID` is not `NULL`. This index can thus support a query for all employees of a specific subsidiary that have no `DATE_OF_BIRTH` value:

```
SELECT first_name, last_name
  FROM employees
 WHERE subsidiary_id = ?
   AND date_of_birth IS NULL
```

Please note that the index covers the entire `where` clause; all filters are used as access predicates during the `INDEX RANGE SCAN`.

We can extend this concept for the original query to find all records where `DATE_OF_BIRTH` `IS NULL`. For that, the `DATE_OF_BIRTH` column has to be the leftmost column in the index so that it can be used as access predicate. Although we do not need a second index column for the query itself, we add another column that can never be `NULL` to make sure the index has all rows. We can use any column that has a `NOT NULL` constraint, like `SUBSIDIARY_ID`, for that purpose.

Alternatively, we can use a constant expression that can never be `NULL`. That makes sure the index has all rows—even if `DATE_OF_BIRTH` is `NULL`.

```
DROP   INDEX emp_dob
```

```
CREATE INDEX emp_dob ON employees (date_of_birth, 'X')
```

Technically, this index is a [function-based index](where-clause-functions.md#case-insensitive-search-using-upper-or-lower). This example also dis­proves the myth that the Oracle database cannot index `NULL`.

> **Tip:**
>
> Add a column that cannot be `NULL` to index `NULL` like any value.


## NOT NULL Constraints

<sub>Source: https://use-the-index-luke.com/sql/where-clause/null/not-null-constraint</sub>

To index an `IS NULL` condition in the Oracle database, the index must have a column that can never be `NULL`.

That said, it is not enough that there are no `NULL` entries. The database has to be sure there can never be a `NULL` entry, otherwise the database must assume that the table has rows that are not in the index.

The following index supports the query only if the column `LAST_NAME` has a `NOT NULL` constraint:

```
DROP INDEX emp_dob
```

```
CREATE INDEX emp_dob_name
          ON employees (date_of_birth, last_name)
```

```
SELECT *
  FROM employees
 WHERE date_of_birth IS NULL
```

Removing the `NOT NULL` constraint renders the index unusable for this query:

```
ALTER TABLE employees MODIFY last_name NULL
```

```
SELECT *
  FROM employees
 WHERE date_of_birth IS NULL
```

> **Tip:**
>
> A missing `NOT NULL` constraint can prevent index usage in an Oracle database—especially for `count(*)` queries.

Besides `NOT NULL` constraints, the database also knows that constant expressions like in the [previous section](#indexing-null) cannot become `NULL`.

An index on a user-defined function, however, does not impose a `NOT NULL` constraint on the index expression:

```
CREATE OR REPLACE FUNCTION blackbox(id IN NUMBER) RETURN NUMBER
DETERMINISTIC
IS BEGIN
   RETURN id;
END
```

```
DROP INDEX emp_dob_name
```

```
CREATE INDEX emp_dob_bb
    ON employees (date_of_birth, blackbox(employee_id))
```

```
SELECT *
  FROM employees
 WHERE date_of_birth IS NULL
```

```
----------------------------------------------------
| Id | Operation         | Name      | Rows | Cost |
----------------------------------------------------
|  0 | SELECT STATEMENT  |           |    1 |  477 |
|* 1 |  TABLE ACCESS FULL| EMPLOYEES |    1 |  477 |
----------------------------------------------------
```

The function name `BLACKBOX` emphasizes the fact that the optimizer has no idea what the function does (see [“*Case-Insensitive Search Using `UPPER` or `LOWER`*”](where-clause-functions.md#case-insensitive-search-using-upper-or-lower)). We can see that the function passes the input value straight through, but for the database it is just a function that returns a number. The `NOT NULL` property of the parameter is lost. Although the index must have all rows, the database does not know that so it cannot use the index for the query.

If *you know* that the function never returns `NULL`, as in this example, you can change the query to reflect that:

```
SELECT *
  FROM employees
 WHERE date_of_birth IS NULL
   AND blackbox(employee_id) IS NOT NULL
```

```
-------------------------------------------------------------
|Id |Operation                   | Name       | Rows | Cost |
-------------------------------------------------------------
| 0 |SELECT STATEMENT            |            |    1 |    3 |
| 1 | TABLE ACCESS BY INDEX ROWID| EMPLOYEES  |    1 |    3 |
|*2 |  INDEX RANGE SCAN          | EMP_DOB_BB |    1 |    2 |
-------------------------------------------------------------
```

The extra condition in the `where` clause is always true and therefore does not change the result. Nevertheless the Oracle database recognizes that you only query rows that must be in the index per definition.

There is, unfortunately, no way to tag a function that never returns `NULL` but you can move the function call to a [virtual column](https://modern-sql.com/caniuse/generated-always-as) (since 11*g*) and put a `NOT NULL` constraint on this column.

```
ALTER TABLE employees ADD bb_expression
      GENERATED ALWAYS AS (blackbox(employee_id)) NOT NULL
```

```
DROP   INDEX emp_dob_bb
```

```
CREATE INDEX emp_dob_bb
    ON employees (date_of_birth, bb_expression)
```

```
SELECT *
  FROM employees
 WHERE date_of_birth IS NULL
   AND blackbox(employee_id) IS NOT NULL
```

```
-------------------------------------------------------------
|Id |Operation                   | Name       | Rows | Cost |
-------------------------------------------------------------
| 0 |SELECT STATEMENT            |            |    1 |    3 |
| 1 | TABLE ACCESS BY INDEX ROWID| EMPLOYEES  |    1 |    3 |
|*2 |  INDEX RANGE SCAN          | EMP_DOB_BB |    1 |    2 |
-------------------------------------------------------------
```

The Oracle database knows that some internal functions only return `NULL` if `NULL` is provided as input.

```
DROP INDEX emp_dob_bb
```

```
CREATE INDEX emp_dob_upname
    ON employees (date_of_birth, upper(last_name))
```

```
SELECT *
  FROM employees
 WHERE date_of_birth IS NULL
```

```
----------------------------------------------------------
|Id |Operation                   | Name           | Cost |
----------------------------------------------------------
| 0 |SELECT STATEMENT            |                |    3 |
| 1 | TABLE ACCESS BY INDEX ROWID| EMPLOYEES      |    3 |
|*2 |  INDEX RANGE SCAN          | EMP_DOB_UPNAME |    2 |
----------------------------------------------------------
```

The `UPPER` function preserves the `NOT NULL` property of the `LAST_NAME` column. Removing the constraint, however, renders the index unusable:

```
ALTER TABLE employees MODIFY last_name NULL
```

```
SELECT *
  FROM employees
 WHERE date_of_birth IS NULL
```

```
----------------------------------------------------
| Id | Operation         | Name      | Rows | Cost |
----------------------------------------------------
|  0 | SELECT STATEMENT  |           |    1 |  477 |
|* 1 |  TABLE ACCESS FULL| EMPLOYEES |    1 |  477 |
----------------------------------------------------
```


## Emulating Partial Indexes in the Oracle Database

<sub>Source: https://use-the-index-luke.com/sql/where-clause/null/partial-index</sub>

The strange way the Oracle database handles `NULL` in indexes can be used to emulate partial indexes. For that, we just have to use `NULL` for rows that should not be indexed.

To demonstrate, we emulate the following partial index:

```
CREATE INDEX messages_todo
          ON messages (receiver)
       WHERE processed = 'N'
```

First, we need a function that returns the `RECEIVER` value only if the `PROCESSED` value is `'N'`.

```
CREATE OR REPLACE
FUNCTION pi_processed(processed CHAR, receiver NUMBER)
RETURN NUMBER
DETERMINISTIC
AS BEGIN
   IF processed IN ('N') THEN
      RETURN receiver;
   ELSE
      RETURN NULL;
   END IF;
END
```

The function must be [deterministic so it can be used in an index definition](where-clause-functions.md#user-defined-functions).

Now we can create an index that contains only the rows having `PROCESSED='N'`.

```
CREATE INDEX messages_todo
          ON messages (pi_processed(processed, receiver))
```

To use the index, you must use the indexed expression in the query:

```
SELECT message
  FROM messages
 WHERE pi_processed(processed, receiver) = ?
```

```
----------------------------------------------------------
|Id | Operation                   | Name          | Cost |
----------------------------------------------------------
| 0 | SELECT STATEMENT            |               | 5330 |
| 1 |  TABLE ACCESS BY INDEX ROWID| MESSAGES      | 5330 |
|*2 |   INDEX RANGE SCAN          | MESSAGES_TODO | 5303 |
----------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
   2 - access("PI_PROCESSED"("PROCESSED","RECEIVER")=:X)
```

> **Sidebar — Partial Indexes, Part II**
>
> As of release 11*g*, there is a second—equally scary—approach to emulating partial indexes in the Oracle database by using an intentionally broken index partition and the [`SKIP_UNUSABLE_INDEXES`](https://docs.oracle.com/en/database/oracle/oracle-database/19/refrn/SKIP_UNUSABLE_INDEXES.html) parameter.
