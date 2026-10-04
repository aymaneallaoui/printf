<!-- Source: https://use-the-index-luke.com/sql/example-schema/sqlite — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# SQLite

<sub>Source: https://use-the-index-luke.com/sql/example-schema/sqlite</sub>

## Contents

1. *[The `where` clause](#sqlite-example-scripts-for-the-where-clause)*
2. *[The Join Operation](#sqlite-example-scripts-for-the-join-operation)*
3. *[3-Minute Test](#sqlite-example-scripts-for-3-minute-quiz)*


## SQLite Example Scripts for “The Where Clause”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/sqlite/where-clause</sub>

### The Equals Operator

#### Surrogate Keys

Creating the `EMPLOYEES` table with 1000 rows.

```
CREATE TABLE employees (
   employee_id   NUMERIC      NOT NULL,
   first_name    VARCHAR(255) NOT NULL,
   last_name     VARCHAR(255) NOT NULL,
   date_of_birth DATE                 ,
   phone_number  VARCHAR(255) NOT NULL,
   junk          CHAR(255)            ,
   CONSTRAINT employees_pk PRIMARY KEY (employee_id)
);
```

```
CREATE VIEW generator_16
AS SELECT 0 n UNION ALL SELECT 1  UNION ALL SELECT 2  UNION ALL
   SELECT 3   UNION ALL SELECT 4  UNION ALL SELECT 5  UNION ALL
   SELECT 6   UNION ALL SELECT 7  UNION ALL SELECT 8  UNION ALL
   SELECT 9   UNION ALL SELECT 10 UNION ALL SELECT 11 UNION ALL
   SELECT 12  UNION ALL SELECT 13 UNION ALL SELECT 14 UNION ALL
   SELECT 15;
```

```
CREATE VIEW generator_256
AS SELECT ( ( hi.n << 4 ) | lo.n ) AS n
     FROM generator_16 lo, generator_16 hi;
```

```
CREATE VIEW generator_4k
AS SELECT ( ( hi.n << 8 ) | lo.n ) AS n
     FROM generator_256 lo, generator_16 hi;
```

```
CREATE VIEW generator_64k
AS SELECT ( ( hi.n << 8 ) | lo.n ) AS n
     FROM generator_256 lo, generator_256 hi;
```

```
INSERT INTO employees (employee_id,  first_name,
                       last_name,    date_of_birth,
                       phone_number, junk)
SELECT gen.n +1,
       CHAR( ABS(random()) % 26 + 65
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           ),
       CHAR( ABS(random()) % 26 + 65
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           ),
       DATE('now', '-' || (abs(random()) % 3650 + 40*365) || ' day'),
       ABS(RANDOM())%9000+1000,
       printf('%.1000c','x')
  FROM generator_4k gen
 WHERE gen.n < 1000;
```

```
UPDATE employees
   SET first_name='MARKUS',
       last_name='WINAND'
 WHERE employee_id=123;
```

```
ANALYZE employees;
```

Note:

- The `GENERATOR_X` views are row generators as described in the [MySQL Row Generator](https://use-the-index-luke.com/blog/2011-07-30/mysql-row-generator) article. In the meanwhile, SQLite supports the [recursive WITH clause (since 3.8.3)](https://modern-sql.com/caniuse/with_recursive_(top_level)) but this approach works with even older versions of SQLite.
- The `JUNK` column is used to have a realistic row length. Without this column the table would become unrealistically small and some demonstrations would not work.
- Random data is filled into the table, with exception to my entry, that is updated after the insert.
- Table and index [statistics](where-clause-the-equals-operator.md#slow-indexes-part-ii) are gathered so that the [optimizer](where-clause-the-equals-operator.md#slow-indexes-part-ii) knows a little bit about the table’s content.

#### Concatenated Keys

```
DROP TABLE employees;
```

```
CREATE TABLE employees (
   employee_id   NUMERIC      NOT NULL,
   first_name    VARCHAR(255) NOT NULL,
   last_name     VARCHAR(255) NOT NULL,
   date_of_birth DATE                 ,
   phone_number  VARCHAR(255) NOT NULL,
   junk          CHAR(255)            ,
   subsidiary_id NUMERIC      NOT NULL,
   CONSTRAINT employees_pk PRIMARY KEY (employee_id, subsidiary_id)
);
```

```
INSERT INTO employees (employee_id,  first_name,
                       last_name,    date_of_birth,
                       phone_number, junk, subsidiary_id)
SELECT gen.n +1,
       CHAR( ABS(random()) % 26 + 65
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           ),
       CHAR( ABS(random()) % 26 + 65
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           ),
       DATE('now', '-' || (abs(random()) % 3650 + 40*365) || ' day'),
       ABS(RANDOM())%9000+1000,
       printf('%.1000c','x'),
       30
  FROM generator_4k gen
 WHERE gen.n < 1000;
```

```
UPDATE employees
   SET first_name='MARKUS',
       last_name='WINAND'
 WHERE employee_id=123
   AND subsidiary_id=30;
```

```
-- generate more records (Very Big Company)
INSERT INTO employees (employee_id,  first_name,
                       last_name,    date_of_birth,
                       phone_number, subsidiary_id, junk)
SELECT gen.n + 1,
       CHAR( ABS(random()) % 26 + 65
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           ),
       CHAR( ABS(random()) % 26 + 65
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           ),
       DATE('now', '-' || (abs(random()) % 3650 + 40*365) || ' day'),
       ABS(RANDOM())%9000+1000,
       CAST(ABS(RANDOM())%(gen.n/9000.0 * 29 + 1) + 1 AS INTEGER),
       printf('%.10c','x')
  FROM generator_64k gen
 WHERE gen.n < 9000;
```

```
ANALYZE employees;
```

Notes:

- As SQLite cannot change primary keys using ALTER TABLE, the entire table is dropped and recreated.
- The new primary key includes by the `SUBSIDIARY_ID`; that is, the `EMPLOYEE_ID` remains in the first position.
- The new records are randomly assigned to the subsidiaries 1 through 29.
- The table and index are analyzed again to make the optimizer aware of the grown data volume.

The next script introduces the index on `SUBSIDIARY_ID` to support the query for all employees of one particular subsidiary:

```
CREATE INDEX emp_sub_id ON employees(subsidiary_id)
```

Although that gives decent performance, it’s better to use the index that supports the primary key:

```
DROP TABLE employees;
```

```
CREATE TABLE employees (
   employee_id   NUMERIC      NOT NULL,
   first_name    VARCHAR(255) NOT NULL,
   last_name     VARCHAR(255) NOT NULL,
   date_of_birth DATE                 ,
   phone_number  VARCHAR(255) NOT NULL,
   junk          CHAR(255)            ,
   subsidiary_id NUMERIC      NOT NULL,
   CONSTRAINT employees_pk PRIMARY KEY (subsidiary_id, employee_id)
);
```

```
INSERT INTO employees (employee_id,  first_name,
                       last_name,    date_of_birth,
                       phone_number, junk, subsidiary_id)
SELECT gen.n +1,
       CHAR( ABS(random()) % 26 + 65
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           ),
       CHAR( ABS(random()) % 26 + 65
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           ),
       DATE('now', '-' || (abs(random()) % 3650 + 40*365) || ' day'),
       ABS(RANDOM())%9000+1000,
       printf('%.1000c','x'),
       30
  FROM generator_4k gen
 WHERE gen.n < 1000;
```

```
UPDATE employees
   SET first_name='MARKUS',
       last_name='WINAND'
 WHERE employee_id=123
   AND subsidiary_id=30;
```

```
-- generate more records (Very Big Company)
INSERT INTO employees (employee_id,  first_name,
                       last_name,    date_of_birth,
                       phone_number, subsidiary_id, junk)
SELECT gen.n + 1,
       CHAR( ABS(random()) % 26 + 65
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           ),
       CHAR( ABS(random()) % 26 + 65
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           , ABS(random()) % 26 + 97
           ),
       DATE('now', '-' || (abs(random()) % 3650 + 40*365) || ' day'),
       ABS(RANDOM())%9000+1000,
       CAST(ABS(RANDOM())%(gen.n/9000.0 * 29 + 1) + 1 AS INTEGER),
       printf('%.10c','x')
  FROM generator_64k gen
 WHERE gen.n < 9000;
```

```
ANALYZE employees;
```

Notes:

- Everything is recreated again, as SQLite doesn’t support modifying the primary key.

### Functions

SQLite doesn’t support `create function` syntax, thus we need to use the expression to calculate the current age directly into the query.

```
SELECT first_name, last_name
     , CAST(STRFTIME('%Y.%m%d', 'now') - STRFTIME('%Y.%m%d', date_of_birth) AS INT)
  FROM employees
 WHERE CAST(STRFTIME('%Y.%m%d', 'now') - STRFTIME('%Y.%m%d', date_of_birth) AS INT) = 42
```

Note that the age is calculated by [formatting the dates as fractional years](https://stackoverflow.com/questions/3123951/sqlite-how-to-calculate-age-from-birth-date/17501785#17501785).

Expressions can be indexed if they are deterministic.

```
CREATE INDEX emp_age ON employees
     ( CAST(STRFTIME('%Y.%m%d', 'now') - STRFTIME('%Y.%m%d', date_of_birth) AS INT) )
```

```
Error: non-deterministic use of strftime() in an index
```


## SQLite Example Scripts for “The Join Operation”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/sqlite/join</sub>

This section contains the `create` and `insert` code to run the examples from [Chapter 4*The Join Operation*](join.md) in an MySQL database.

```
CREATE TABLE sales (
  sale_id       NUMERIC NOT NULL,
  employee_id   NUMERIC NOT NULL,
  subsidiary_id NUMERIC NOT NULL,
  sale_date     DATE   NOT NULL,
  eur_value     NUMERIC(17,2) NOT NULL,
  product_id    NUMERIC NOT NULL,
  quantity      NUMERIC NOT NULL,
  junk          CHAR(200),
  CONSTRAINT sales_pk
     PRIMARY KEY (sale_id),
  CONSTRAINT sales_emp_fk
     FOREIGN KEY          (subsidiary_id, employee_id)
      REFERENCES employees(subsidiary_id, employee_id)
);
```

```
INSERT INTO sales (sale_id
                 , subsidiary_id, employee_id
                 , sale_date, eur_value
                 , product_id, quantity
                 , junk)
SELECT data.*
  FROM (
       SELECT ((e.subsidiary_id * 10001 + e.employee_id) * 1801) + gen.n AS sale_id
            , e.subsidiary_id, e.employee_id
            , DATE('now', '-' || (abs(random()) % 3650) || ' day') sale_date
            , (ABS(RANDOM())%9990)/100 AS eur_value

            , ABS(RANDOM())%25+1 product_id
            , ABS(RANDOM())%15+1 quantity
            , 'junk'
         FROM employees e
         JOIN ( SELECT generator_4k.n+1 n
                  FROM generator_4k
                 WHERE generator_4k.n < 1800
              ) gen
           ON gen.n < employee_id / 5
        WHERE employee_id % 7 = 4
       ) data
  WHERE strftime('%w', sale_date) NOT IN (0,6)
  ORDER BY sale_date;
```

Notes:

- The rows are inserted chronologically to reflect a natural table growth.
- Only a small fraction of employees have sales at all.


## SQLite Example Scripts for “3-Minute Quiz”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/sqlite/3-minute-quiz</sub>

This section contains the `create`, `insert` and `select` statements for the “[Test your SQL Know-How in 3 Minutes](https://use-the-index-luke.com/3-minute-quiz)” test. You may want to [test yourself](https://use-the-index-luke.com/3-minute-quiz) before reading this page.

The `create` and `insert` statements are available in the [example schema archive](https://use-the-index-luke.com/use-the-index-luke.tar.gz).

The execution plans are formatted for better readability.

### Question 1 — DATE Anti-Pattern

```
CREATE INDEX tbl_idx ON tbl (date_column);
```

```
SELECT COUNT(*)
  FROM tbl
 WHERE STRFTIME ('%Y', date_column) = 2025;
```

```
SELECT COUNT(*)
  FROM tbl
 WHERE date_column >= '2025-01-01'
   AND date_column <  '2026-01-01';
```

The first execution plan performs a full index scan (`SCAN TABLE`). The second execution plan, on the other hand, uses the index properly (`SEARCH` is the important part here).

```
0|0|0|SCAN TABLE tbl USING COVERING INDEX tbl_idx
```

```
0|0|0|SEARCH TABLE tbl USING COVERING INDEX tbl_idx
                  (date_column>? AND date_column<?)
```

> **Learn More:**
>
> - [Using Functions in the `WHERE` clause](where-clause-functions.md)
> - [Common Anti-Patterns: `DATE`](where-clause-obfuscation.md#date-types)
> - [Reading PostgreSQL explain plan output](explain-plan-postgresql.md#operations)

### Question 2 — Indexed Top-N

```
CREATE INDEX tbl_idx ON tbl (a, date_column);
```

```
SELECT *
  FROM tbl
 WHERE a = 12
 ORDER BY date_column DESC
 LIMIT 1;
```

The query uses the index and no `SORT` operation.

```
0|0|0|SEARCH TABLE tbl USING INDEX tbl_idx (a=?)
```

### Question 3 — Column Order

```
CREATE INDEX tbl_idx ON tbl (a, b);
```

```
SELECT *
  FROM tbl
 WHERE a = 38
   AND b = 1;
```

```
SELECT *
  FROM tbl
 WHERE b = 1;
```

```
DROP INDEX tbl_idx ;
```

```
CREATE INDEX tbl_idx ON tbl (b, a);
```

```
SELECT *
  FROM tbl
 WHERE a = 38
   AND b = 1;
```

```
SELECT *
  FROM tbl
 WHERE b = 1;
```

The first query can use both indexes efficiently:

```
0|0|0|SEARCH TABLE tbl USING INDEX tbl_idx (a=? AND b=?)
```

```
0|0|0|SEARCH TABLE tbl USING INDEX tbl_idx (b=? AND a=?)
```

The second query cannot use the first index and reads the entire table (`SCAN TABLE`) instead:

```
0|0|0|SCAN TABLE tbl
```

Changing the column order allows the second query to use the index too:

```
0|0|0|SEARCH TABLE tbl USING INDEX tbl_idx (b=?)
```

> **Learn More:**
>
> - [The column order in multi-column indexes](where-clause-the-equals-operator.md#concatenated-indexes)

### Question 4 — LIKE

```
CREATE INDEX tbl_idx ON tbl (text);
```

```
PRAGMA case_sensitive_like = true;
```

```
SELECT *
  FROM tbl
 WHERE text LIKE 'TJ%';
```

The query can use the index efficiently.

```
0|0|0|SEARCH TABLE tbl USING INDEX tbl_idx (text>? AND text<?)
```

Note that this example requires the above `pragma` setting: otherwise, [`like` works case-insensitive in SQLite](https://sqlite.org/pragma.html#pragma_case_sensitive_like) and cannot use this index for this query.

> **Learn More:**
>
> - [A visual explanation why `LIKE` is slow](where-clause-searching-for-ranges.md#indexing-like-filters)

### Question 5 — Index Only Scan

```
CREATE INDEX tbl_idx ON tbl (a, date_column);
```

```
SELECT date_column, count(*)
  FROM tbl
 WHERE a = 38
 GROUP BY date_column;
```

```
SELECT date_column, count(*)
  FROM tbl
 WHERE a = 38
   AND b = 1
 GROUP BY date_column;
```

Both indexes are used, of course. The difference is that the first query doesn’t access the table (`COVERING`), so the first query is much faster.

```
0|0|0|SEARCH TABLE tbl USING COVERING INDEX tbl_idx (a=?)
```

```
0|0|0|SEARCH TABLE tbl USING INDEX tbl_idx (a=?)
```

The second query must be considerably slower because every row needs a table access — also for those that are filtered by the new condition. Even if the index has a low [clustering factor](clustering.md#index-filter-predicates-used-intentionally), it is still about twice as many blocks to read.

> **Learn More:**
>
> - [Chapter 5*Clustering Data: The Second Power of Indexing*](clustering.md)
