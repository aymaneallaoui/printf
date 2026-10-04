<!-- Source: https://use-the-index-luke.com/sql/example-schema/db2 — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# Db2 (LUW) Example Scripts

<sub>Source: https://use-the-index-luke.com/sql/example-schema/db2</sub>

The scripts provided in this appendix are ready to run and were tested on Db2 (LUW) Express-C 9.7 through 11.5.

## Contents

1. *[The `where` clause](#db2-luw-example-scripts-for-the-where-clause)*
2. *[Testing and Scalability](#db2-luw-example-scripts-for-testing-and-scalability)*
3. *[The Join Operation](#db2-luw-example-scripts-for-the-join-operation)*
4. *[3-Minute Test](#db2-luw-example-scripts-for-3-minute-quiz)*


## Db2 (LUW) Example Scripts for “The Where Clause”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/db2/where-clause</sub>

### The Equals Operator

#### Surrogate Keys

The following script creates the `EMPLOYEES` table with 1000 entries.

The script is intended to be run from the `db2` command line. It uses the semicolon (;) as the regular statement terminator, but two semicolons (;;) to terminate PL/SQL code.

```
CREATE TABLE employees (
   employee_id   NUMERIC       NOT NULL,
   first_name    VARCHAR(1000) NOT NULL,
   last_name     VARCHAR(1000) NOT NULL,
   date_of_birth DATE                  ,
   phone_number  VARCHAR(1000) NOT NULL,
   junk          CHAR(254)             ,
   CONSTRAINT employees_pk PRIMARY KEY (employee_id)
);
```

```
--#SET TERMINATOR ;;
CREATE FUNCTION random_string(minlen NUMERIC, maxlen NUMERIC)
RETURNS VARCHAR(1000)
LANGUAGE SQL
NOT DETERMINISTIC
NO EXTERNAL ACTION
READS SQL DATA
BEGIN
  DECLARE rv  VARCHAR(1000) DEFAULT '';
  DECLARE i   NUMERIC       DEFAULT 0;
  DECLARE len NUMERIC       DEFAULT 0;

  IF maxlen < 1 OR minlen < 1 OR maxlen < minlen THEN
    RETURN NULL;
  END IF;

  SET i = floor(rand()*(maxlen-minlen)) + minlen;
  WHILE (i > 0)  DO
    SET rv = rv || chr(97+CAST(rand() * 25 AS INTEGER));
    SET i  =  i - 1;
  END WHILE;
  RETURN rv;
END
;;
--#SET TERMINATOR ;
```

```
INSERT INTO employees (employee_id,  first_name,
                       last_name,    date_of_birth,
                       phone_number, junk)
WITH generator (n) AS
( SELECT 1 n   FROM sysibm.sysdummy1
   UNION ALL
  SELECT n + 1 FROM generator
   WHERE n < 1000
)
SELECT generator.n
     , initcap(lower(random_string(2, 8)))
     , initcap(lower(random_string(2, 8)))
     , CURRENT_DATE - floor(rand() * 365 * 10 + 40 * 365) days
     , floor(rand() * 9000 + 1000)
     , 'junk'
  FROM generator;
```

```
UPDATE employees
   SET first_name='MARKUS',
       last_name='WINAND'
 WHERE employee_id=123;
```

```
RUNSTATS ON TABLE employees;
```

Notes:

- The `JUNK` column is used to have a realistic row length. Because it’s data type is `CHAR`, as opposed to `VARCHAR`, it always needs the 254 bytes it can hold (254 is the limit on Db2 (LUW) Express-C 10.5). Without this column the table would become unrealistically small and many demonstrations would not work.
- Random data is filled into the table, with exception to my entry, that is updated after the insert.
- Table [statistics](where-clause-the-equals-operator.md#slow-indexes-part-ii) are gathered so that the [optimizer](where-clause-the-equals-operator.md#slow-indexes-part-ii) knows a little bit about the table’s content.
- [Before version 10](https://www.ibm.com/docs/en/db2/11.5.x?topic=commands-runstats) Db2 needs a fully qualified table name (including schema) for `RUNSTATS`. If you are getting an error, try adding the schema name. You can query for the CURRENT_SCHEMA like this:

  ```
  SELECT current_schema FROM sysibm.sysdummy1;
  ```

#### Concatenated Keys

This script changes the `EMPLOYEES` table so that it reflects the situation after the merger with Very Big Company:

```
--#SET TERMINATOR ;

-- add subsidiary_id and update existing records
ALTER TABLE employees ADD subsidiary_id NUMERIC;
UPDATE      employees SET subsidiary_id = 30;
ALTER TABLE employees ALTER COLUMN subsidiary_id SET NOT NULL;

-- change the PK
ALTER TABLE employees DROP PRIMARY KEY;
-- to prevent failur for reason "7"
REORG TABLE employees;
ALTER TABLE employees ADD CONSTRAINT employees_pk
      PRIMARY KEY (employee_id, subsidiary_id);

-- generate more records (Very Big Company)
INSERT INTO employees (employee_id,  first_name,
                       last_name,    date_of_birth,
                       phone_number, subsidiary_id, junk)
WITH generator (n) AS
( SELECT 1001 n   FROM sysibm.sysdummy1
   UNION ALL
  SELECT n + 1 FROM generator
   WHERE n < 10000
)
SELECT generator.n
     , initcap(lower(random_string(2, 8)))
     , initcap(lower(random_string(2, 8)))
     , CURRENT_DATE - floor(rand() * 365 * 10 + 40 * 365) days
     , floor(rand() * 9000 + 1000)
     , floor(rand() * least(mod(generator.n, 2)+0.2,1) * (generator.n-1000)/9000*29)
     , 'junk'
  FROM generator;

RUNSTATS ON TABLE employees;
```

Notes:

- The new primary key just extended by the `SUBSIDIARY_ID`; that is, the `EMPLOYEE_ID` remains in the first position.
- The new records are randomly assigned to the subsidiaries 1 through 29.
- The table and index are analyzed again to make the optimizer aware of the grown data volume.

The next script introduces the index on `SUBSIDIARY_ID` to support the query for all employees of one particular subsidiary:

```
--#SET TERMINATOR ;
CREATE INDEX emp_sub_id ON employees (subsidiary_id);
```

Although that gives decent performance, it’s better to use the index that supports the primary key:

```
--#SET TERMINATOR ;

-- index to support the new PK
CREATE UNIQUE INDEX employees_pk_new
    ON employees (subsidiary_id, employee_id);

ALTER TABLE employees
 DROP PRIMARY KEY;

-- this will automatically use the new index:
-- SQL0598W  Existing index "EMPLOYEE_PK_NEW" is used as the index for
-- the primary key or a unique key.  SQLSTATE=01550
ALTER TABLE employees
  ADD CONSTRAINT employees_pk
      PRIMARY KEY (subsidiary_id, employee_id);
-- cleanup
RENAME INDEX employees_pk_new TO employees_pk;
DROP INDEX emp_sub_id;
```

Notes:

- A new index is created and used to support the PK.
- Dropping the primary key automatically drops the automatically created index supporting it.
- Adding the new primary key makes use of the new index automatically.

### Functions (Db2 10.5+)

#### Case-Insensitive Search

The randomized names were already created in correct case, just update “my” record:

```
--#SET TERMINATOR ;

UPDATE employees
   SET first_name = 'Markus'
     , last_name  = 'Winand'
 WHERE employee_id   = 123
   AND subsidiary_id = 30;
```

The statement to create the function-based index:

```
--#SET TERMINATOR ;
CREATE INDEX emp_up_name
    ON employees (UPPER(last_name));
DROP INDEX emp_name;
```

```
RUNSTATS ON TABLE employees;
```

#### User-Defined Functions

Define a function that calculates the age and attempts to use it in an index:

```
--#SET TERMINATOR ;;
CREATE FUNCTION get_age(date_of_birth DATE)
RETURNS NUMERIC
LANGUAGE SQL
BEGIN
    RETURN YEAR(CURRENT_DATE - date_of_birth);
END
;;
--#SET TERMINATOR ;

CREATE INDEX invalid ON EMPLOYEES (get_age(date_of_birth));
```

You should get the error “*SQL0356N: The index was not created because a key expression was invalid. Key expression: "1". Reason code: "5”*". Whereas [reason code 5 means](https://www.ibm.com/docs/en/db2/11.5.x?topic=messages-sql0000-0999#sqlmsg__SQL0356N): "*The key expression included a user-defined function.*"

### Emulating Partial Indexes

#### Setup

```
--#SET TERMINATOR ;

CREATE TABLE messages (
       id         NUMERIC(10,0) NOT NULL,
       processed  CHAR(1)       NOT NULL,
       receiver   NUMERIC(10,0) NOT NULL,
       message    CHAR(200)     NOT NULL,

       CONSTRAINT messages_pk PRIMARY KEY (id)
);

INSERT INTO messages (id, processed, receiver, message)
WITH generator(n) AS
( SELECT 1 n   FROM sysibm.sysdummy1
   UNION ALL
  SELECT n + 1 FROM generator
   WHERE n < 999999
)
SELECT n id
     , CASE WHEN rand() < 0.09 THEN 'N' ELSE 'Y' END processed
     , floor(rand() * 100) receiver
     , 'junk' message
  FROM generator;

RUNSTATS ON TABLE messages;

DROP INDEX messages_not_processed_pi;
CREATE INDEX messages_not_processed_pi
    ON messages (CASE WHEN processed = 'N' THEN receiver+0
                                           ELSE NULL
                 END)
EXCLUDE NULL KEYS;

SELECT *
  FROM messages
 WHERE (CASE WHEN processed = 'N' THEN receiver+0
                                  ELSE NULL
         END) = ?;
```

#### Regular Attempt

```
CREATE INDEX messages_not_processed_pi
    ON messages (CASE WHEN processed = 'N' THEN receiver
                                           ELSE NULL
                 END)
EXCLUDE NULL KEYS;

SELECT *
  FROM messages
 WHERE (CASE WHEN processed = 'N' THEN receiver
                                  ELSE NULL
         END) = ?;
```

```
Explain Plan
-------------------------------------------------------
ID | Operation        |                    Rows |  Cost
 1 | RETURN           |                         | 49686
 2 |  TBSCAN MESSAGES | 900 of 999999 (   .09%) | 49686

Predicate Information
 2 - SARG (Q1.PROCESSED = 'N')
     SARG (Q1.RECEIVER = ?)
```

Same happens when using an expression like `CASE processed WHEN 'N'…`.

#### Obfuscated Attempt

```
DROP INDEX messages_not_processed_pi;
CREATE INDEX messages_not_processed_pi
    ON messages (CASE WHEN processed = 'N' THEN receiver+0
                                           ELSE NULL
                 END)
EXCLUDE NULL KEYS;

SELECT *
  FROM messages
 WHERE (CASE WHEN processed = 'N' THEN receiver+0
                                  ELSE NULL
         END) = ?;
```

```
ID | Operation                            |                      Rows |  Cost
 1 | RETURN                               |                           | 13071
 2 |  FETCH MESSAGES                      |  40000 of 40000 (100.00%) | 13071
 3 |   RIDSCN                             |  40000 of 40000 (100.00%) |  1665
 4 |    SORT (UNQIUE)                     |  40000 of 40000 (100.00%) |  1665
 5 |     IXSCAN MESSAGES_NOT_PROCESSED_PI | 40000 of 999999 (  4.00%) |  1646

Predicate Information
 2 - SARG ( CASE WHEN (Q1.PROCESSED = 'N') THEN (Q1.RECEIVER + 0) ELSE NULL END = ?)
 5 - START ( CASE WHEN (Q1.PROCESSED = 'N') THEN (Q1.RECEIVER + 0) ELSE NULL END = ?)
      STOP ( CASE WHEN (Q1.PROCESSED = 'N') THEN (Q1.RECEIVER + 0) ELSE NULL END = ?)
```


## Db2 (LUW) Example Scripts for “Testing and Scalability”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/db2/performance-testing-scalability</sub>

This section contains the `create`, `insert` and PL/SQL code to run the scalability test from [Chapter 3*Performance and Scalability*](testing-scalability.md) in an Db2 (LUW) database.

> **Warning:**
>
> These scripts will create large objects in the database and produce a huge amount of redo logs.

```
--#SET TERMINATOR ;

-- Disable autocommit
UPDATE COMMAND OPTIONS USING C OFF;

CREATE TABLE scale_data (
   section NUMERIC(10,0) NOT NULL,
   id1     NUMERIC(10,0) NOT NULL,
   id2     NUMERIC(10,0) NOT NULL
) NOT LOGGED INITIALLY;
```

Note:

- Auto commit is disabled to disable logging for this table. The commit is done in the next step.
- There is no primary key (to keep the data generation simple)
- There is no index (yet). That’s done after filling the table
- There is no “junk” column because the table is actually not accessed during testing

```
--#SET TERMINATOR ;

INSERT INTO scale_data (section, id1, id2)
WITH sections (n) AS
( SELECT 1 n   FROM sysibm.sysdummy1
   UNION ALL
  SELECT n + 1 FROM sections
   WHERE n < 300
)
, gen (n) AS
( SELECT 1 n   FROM sysibm.sysdummy1
   UNION ALL
  SELECT n + 1 FROM gen
   WHERE n < 900000
)
SELECT sections.n, gen.n, FLOOR(rand() * 100)
  FROM sections
     , gen
 WHERE gen.n < sections.n * 3000;

COMMIT;
```

Note:

- This code generates 300 sections, you may need to adjust the number for your environment. If you increase the number of sections, you must also increase the second generator. It must generate at least `3000 x <number of sections>` records.
- The table will need some gigabytes

```
--#SET TERMINATOR ;

-- Disable autocommit
UPDATE COMMAND OPTIONS USING C OFF;

ALTER TABLE scale_data ACTIVATE NOT LOGGED INITIALLY;

CREATE INDEX scale_slow ON scale_data (section, id1, id2);

COMMIT;

RUNSTATS ON TABLE scale_data;
```

Note:

- The index will also need some gigabytes
- [Before version 10](https://www.ibm.com/docs/en/db2/11.5.x?topic=commands-runstats) Db2 needs a fully qualified table name (including schema) for `RUNSTATS`. If you are getting an error, try adding the schema name. You can query for the CURRENT_SCHEMA like this:

  ```
  SELECT current_schema FROM sysibm.sysdummy1;
  ```


## Db2 (LUW) Example Scripts for “The Join Operation”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/db2/join</sub>

This section contains the `create` and `insert` statements to run the examples from [Chapter 4*The Join Operation*](join.md) in an IBM Db2 (LUW) database.

```
--#SET TERMINATOR ;

-- Disable autocommit
UPDATE COMMAND OPTIONS USING C OFF;

CREATE TABLE sales (
  sale_id       NUMERIC(10,0) NOT NULL,
  employee_id   NUMERIC(10,0) NOT NULL,
  subsidiary_id NUMERIC(10,0) NOT NULL,
  sale_date     DATE          NOT NULL,
  eur_value     NUMERIC(17,2) NOT NULL,
  product_id    NUMERIC(10,0) NOT NULL,
  quantity      NUMERIC(10,0) NOT NULL,
  junk          CHAR(200),
  CONSTRAINT sales_pk
     PRIMARY KEY (sale_id),
  CONSTRAINT sales_emp_fk
     FOREIGN KEY          (subsidiary_id, employee_id)
      REFERENCES employees(subsidiary_id, employee_id)
) NOT LOGGED INITIALLY;

INSERT INTO sales (sale_id
                 , subsidiary_id, employee_id
                 , sale_date, eur_value
                 , product_id, quantity
                 , junk)
WITH generator (n) AS
( SELECT 1 n   FROM sysibm.sysdummy1
   UNION ALL
  SELECT n + 1 FROM generator
   WHERE n < 1800
)
SELECT row_number() OVER (), data.*
  FROM (
       SELECT e.subsidiary_id, e.employee_id
            , CURRENT_DATE - floor(rand() * 365 * 10 ) days sale_date
            , CAST(rand()*1000 AS NUMERIC(17,2)) eur_value
            , CAST(rand()*25 AS NUMERIC(2,0)) + 1 product_id
            , CAST(rand()*5 AS NUMERIC(1,0)) + 1 quantity
            , 'junk'
         FROM employees e
            , generator gen
        WHERE MOD(employee_id, 7) = 4
          AND gen.n < employee_id / 5
        ORDER BY sale_date
       ) data
 WHERE dayofweek(sale_date) NOT IN (1,7);

COMMIT;

CREATE INDEX sales_sub_emp ON sales (subsidiary_id, employee_id);

RUNSTATS ON TABLE sales;
```

Notes:

- Logging is disabled (which required disabling auto commit) to preserve space and time.
- The rows are inserted chronologically to reflect a natural table growth.
- Only a small fraction of employees have sales at all.
- No sales on Sundays.
- [Before version 10](https://www.ibm.com/docs/en/db2/11.5.x?topic=commands-runstats) Db2 needs a fully qualified table name (including schema) for `RUNSTATS`. If you are getting an error, try adding the schema name. You can query for the CURRENT_SCHEMA like this:

  ```
  SELECT current_schema FROM sysibm.sysdummy1;
  ```


## Db2 (LUW) Example Scripts for “3-Minute Quiz”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/db2/3-minute-quiz</sub>

This section contains the `create`, `insert` and `select` statements for the “[Test your SQL Know-How in 3 Minutes](https://use-the-index-luke.com/3-minute-quiz)” test. You may want to test yourself before reading this page.

The `create` and `insert` statements are available in the [example schema archive](https://use-the-index-luke.com/use-the-index-luke.tar.gz).

The execution plans shown are abbreviated for better readability.

### Question 1 — DATE Anti-Pattern

```
--#SET TERMINATOR ;
CREATE INDEX tbl_idx ON tbl (date_column);
```

```
--#SET TERMINATOR ;
SELECT COUNT(*)
  FROM tbl
 WHERE TO_CHAR(date_column, 'YYYY') = 2025;
```

```
--#SET TERMINATOR ;
SELECT COUNT(*)
  FROM tbl
 WHERE date_column >= DATE'2025-01-01'
   AND date_column <  DATE'2026-01-01';
```

The first query performs a full index scan (`IXSCAN` with no `START`/`STOP` (but `SARG`) in the Predicate Information)

```
Explain Plan
-----------------------------------------------------
ID | Operation          |                 Rows | Cost
 1 | RETURN             |                      |   27
 2 |  GRPBY (COMPLETE)  |    1 of 40 (  2.50%) |   27
 3 |   IXSCAN TBL_IDX   | 40 of 1000 (  4.00%) |   27

Predicate Information
 3 - SARG ( TO_CHAR(TIMESTAMP(Q1.DATE_COLUMN, 0), 'YYYY') = '2017')
```

The second query can do an index range scan (`START`/`STOP` at the `IXSCAN` operation).

```
Explain Plan
------------------------------------------------------
ID | Operation          |                  Rows | Cost
 1 | RETURN             |                       |    6
 2 |  GRPBY (COMPLETE)  |    1 of 100 (  1.00%) |    6
 3 |   IXSCAN TBL_IDX   | 100 of 1000 ( 10.00%) |    6

Predicate Information
 3 - START ('01/01/2017' <= Q1.DATE_COLUMN)
      STOP (Q1.DATE_COLUMN < '01/01/2018')
```

Note that Db2 (LUW) properly optimize [`extract` expressions](https://modern-sql.com/feature/extract):

```
SELECT text, date_column
  FROM tbl
 WHERE EXTRACT(YEAR FROM date_column) = 2017
```

```
Explain Plan
----------------------------------------------------
ID | Operation          |                Rows | Cost
 1 | RETURN             |                     |   34
 2 |  FETCH TBL         |  19 of 19 (100.00%) |   34
 3 |   RIDSCN           |  19 of 19 (100.00%) |    6
 4 |    SORT (UNIQUE)   |  19 of 19 (100.00%) |    6
 5 |     IXSCAN TBL_IDX | 19 of 186 ( 10.22%) |    6

Predicate Information
 2 - SARG ('01/01/2017' <= Q1.DATE_COLUMN)
     SARG (Q1.DATE_COLUMN <= '12/31/2017')
 5 - START ('01/01/2017' <= Q1.DATE_COLUMN)
      STOP (Q1.DATE_COLUMN <= '12/31/2017')
```

> **Learn More:**
>
> - [Using Functions in the `WHERE` clause](where-clause-functions.md)
> - [Common Anti-Patterns: `DATE`](where-clause-obfuscation.md#date-types)
> - [Reading Db2 (LUW) explain plan output](explain-plan-db2.md#db2-luw-execution-plan-operations)

### Question 2 — Indexed Top-N

```
--#SET TERMINATOR ;
CREATE INDEX tbl_idx ON tbl (a, date_column);
```

```
--#SET TERMINATOR ;
SELECT *
  FROM tbl
 WHERE a = 12
 ORDER BY date_column DESC
 FETCH FIRST 1 ROW ONLY;
```

The query uses the index (`IXSCAN`) and fetches in reverse order (`REVERSE`). Note that there is no sort operation.

```
Explain Plan
-----------------------------------------------------------
ID | Operation                  |               Rows | Cost
 1 | RETURN                     |                    |   13
 2 |  FETCH TBL                 |   1 of 7 ( 14.29%) |   21
 3 |   IXSCAN (REVERSE) TBL_IDX | 7 of 186 (  3.76%) |    6

Predicate Information
 3 - START (Q1.A = +00012.)
      STOP (Q1.A = +00012.)
```

### Question 3 — Column Order

```
--#SET TERMINATOR ;
CREATE INDEX tbl_idx ON tbl (a, b);
```

```
--#SET TERMINATOR ;
SELECT *
  FROM tbl
 WHERE a = 38
   AND b = 1;
```

```
--#SET TERMINATOR ;
SELECT *
  FROM tbl
 WHERE b = 1;
```

```
--#SET TERMINATOR ;
DROP INDEX tbl_idx ;
```

```
--#SET TERMINATOR ;
CREATE INDEX tbl_idx ON tbl (b, a);
```

```
--#SET TERMINATOR ;
SELECT *
  FROM tbl
 WHERE a = 38
   AND b = 1;
```

```
--#SET TERMINATOR ;
SELECT *
  FROM tbl
 WHERE b = 1;
```

The first query can use both indexes efficiently (`IXSCAN` with `START` and `STOP` predicates):

```
Explain Plan
-------------------------------------------------
ID | Operation        |               Rows | Cost
 1 | RETURN           |                    |   21
 2 |  FETCH TBL       |   7 of 7 (100.00%) |   21
 3 |   IXSCAN TBL_IDX | 7 of 186 (  3.76%) |    6

Predicate Information
 3 - START (Q1.A = +00038.)
     START (Q1.B = +00001.)
      STOP (Q1.A = +00038.)
      STOP (Q1.B = +00001.)
```

```
Explain Plan
-------------------------------------------------
ID | Operation        |               Rows | Cost
 1 | RETURN           |                    |   21
 2 |  FETCH TBL       |   7 of 7 (100.00%) |   21
 3 |   IXSCAN TBL_IDX | 7 of 186 (  3.76%) |    6

Predicate Information
 3 - START (Q1.B = +00001.)
     START (Q1.A = +00038.)
      STOP (Q1.B = +00001.)
      STOP (Q1.A = +00038.)
```

The second query cannot use the first index—the query performs a full table scan (`TBSCAN`):

```
Explain Plan
----------------------------------------------
ID | Operation   |                 Rows | Cost
 1 | RETURN      |                      |   99
 2 |  TBSCAN TBL | 40 of 1000 (  4.00%) |   99

Predicate Information
 2 - SARG (Q1.B = +00001.)
```

Reversing the column order in the index allows the second query to use the index efficiently too (`IXSCAN` with `START` and `STOP` predicates):

```
Explain Plan
-----------------------------------------------------
ID | Operation          |                 Rows | Cost
 1 | RETURN             |                      |   34
 2 |  FETCH TBL         |   40 of 40 (100.00%) |   34
 3 |   RIDSCN           |   40 of 40 (100.00%) |    6
 4 |    SORT (UNIQUE)   |   40 of 40 (100.00%) |    6
 5 |     IXSCAN TBL_IDX | 40 of 1000 (  4.00%) |    6

Predicate Information
 2 - SARG (Q1.B = +00001.)
 5 - START (Q1.B = +00001.)
      STOP (Q1.B = +00001.)
```

> **Tip:**
>
> - [The column order in multi-column indexes](where-clause-the-equals-operator.md#concatenated-indexes)

### Question 4 — LIKE

```
--#SET TERMINATOR ;
CREATE INDEX tbl_idx ON tbl (text);
```

```
--#SET TERMINATOR ;
SELECT *
  FROM tbl
 WHERE text LIKE 'TJ%';
```

The query can use the index efficiently (`IXSCAN` with `START` and `STOP` predicates):

```
Explain Plan
-------------------------------------------------
ID | Operation        |               Rows | Cost
 1 | RETURN           |                    |   13
 2 |  FETCH TBL       |   1 of 1 (100.00%) |   13
 3 |   IXSCAN TBL_IDX | 1 of 300 (   .33%) |    6

Predicate Information
 3 - START ('TJ..................................
      STOP (Q1.TEXT <= 'TJ.......................
```

> **Tip:**
>
> - [A visual explanation when `LIKE` is slow](where-clause-searching-for-ranges.md#indexing-like-filters)

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

Both indexes are used, of course. The difference is that the first query doesn’t access the table, so the first query is much faster.

```
Explain Plan
---------------------------------------------------
ID | Operation          |               Rows | Cost
 1 | RETURN             |                    |    0
 2 |  GRPBY (COMPLETE)  |   2 of 2 (100.00%) |    0
 3 |   IXSCAN TBL_IDX   | 2 of 300 (   .67%) |    0

Predicate Information
 3 - START (Q1.A = +00038.)
      STOP (Q1.A = +00038.)
```

The second query must be considerably slower because every row needs a table access — also for those that are filtered by the new condition. Even if the index has a low [clustering factor](clustering.md#index-filter-predicates-used-intentionally), it is still about twice as many blocks to read.

```
Explain Plan
---------------------------------------------------
ID | Operation          |               Rows | Cost
 1 | RETURN             |                    |    6
 2 |  GRPBY (COMPLETE)  |             0 of 0 |    6
 3 |   FETCH TBL        |   0 of 2 (   .00%) |    6
 4 |    IXSCAN TBL_IDX  | 2 of 300 (   .67%) |    0

Predicate Information
 3 - SARG (Q1.B = +00001.)
 4 - START (Q1.A = +00038.)
      STOP (Q1.A = +00038.)
```

> **Tip:**
>
> - [Chapter 5*Clustering Data: The Second Power of Indexing*](clustering.md)
