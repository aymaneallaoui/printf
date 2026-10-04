<!-- Source: https://use-the-index-luke.com/sql/example-schema/postgresql — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# PostgreSQL Example Scripts

<sub>Source: https://use-the-index-luke.com/sql/example-schema/postgresql</sub>

The scripts provided in this appendix are ready to run and were tested on the PostgreSQL database release 9.0.3. Most of the examples will also work on earlier releases.

## Contents

1. *[The `where` clause](#postgresql-example-scripts-for-the-where-clause)*
2. *[Testing and Scalability](#postgresql-example-scripts-for-testing-and-scalability)*
3. *[The Join Operation](#postgresql-example-scripts-for-the-join-operation)*
4. *[Sorting and Grouping](#postgresql-example-scripts-for-sorting-and-grouping)*
5. *[Partial Results](#postgresql-example-scripts-for-partial-results)*
6. *[Insert, Delete and Update](#postgresql-example-scripts-for-insert-delete-and-update)*
7. *[3-Minute Test](#postgresql-example-scripts-for-3-minute-quiz)*


## PostgreSQL Example Scripts for “The Where Clause”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/postgresql/where-clause</sub>

The scripts provided in this appendix are ready to run and were tested on the PostgreSQL 9. Most of the examples will also work earlier releases.

### The Equals Operator

#### Surrogate Keys

The following script creates the `EMPLOYEES` table with 1000 entries.

```
CREATE TABLE employees (
   employee_id   NUMERIC       NOT NULL,
   first_name    VARCHAR(1000) NOT NULL,
   last_name     VARCHAR(1000) NOT NULL,
   date_of_birth DATE                  ,
   phone_number  VARCHAR(1000) NOT NULL,
   junk          CHAR(1000)            ,
   CONSTRAINT employees_pk PRIMARY KEY (employee_id)
);
```

```
CREATE FUNCTION random_string(minlen NUMERIC, maxlen NUMERIC)
RETURNS VARCHAR(1000)
AS
$$
DECLARE
  rv VARCHAR(1000) := '';
  i  INTEGER := 0;
  len INTEGER := 0;
BEGIN
  IF maxlen < 1 OR minlen < 1 OR maxlen < minlen THEN
    RETURN rv;
  END IF;

  len := floor(random()*(maxlen-minlen)) + minlen;

  FOR i IN 1..floor(len) LOOP
    rv := rv || chr(97+CAST(random() * 25 AS INTEGER));
  END LOOP;
  RETURN rv;
END;
$$ LANGUAGE plpgsql;
```

```
INSERT INTO employees (employee_id,  first_name,
                       last_name,    date_of_birth,
                       phone_number, junk)
SELECT GENERATE_SERIES
     , initcap(lower(random_string(2, 8)))
     , initcap(lower(random_string(2, 8)))
     , CURRENT_DATE - CAST(floor(random() * 365 * 10 + 40 * 365) AS NUMERIC) * INTERVAL '1 DAY'
     , CAST(floor(random() * 9000 + 1000) AS NUMERIC)
     , 'junk'
  FROM GENERATE_SERIES(1, 1000);
```

```
UPDATE employees
   SET first_name='MARKUS',
       last_name='WINAND'
 WHERE employee_id=123;
```

```
VACUUM ANALYZE employees;
```

Notes:

- The `JUNK` column is used to have a realistic row length. Because it’s data type is `CHAR`, as opposed to `VARCHAR`, it always needs the 1000 bytes it can hold. Without this column the table would become unrealistically small and many demonstrations would not work.
- Random data is filled into the table, with exception to my entry, that is updated after the insert.
- Table [statistics](where-clause-the-equals-operator.md#slow-indexes-part-ii) are gathered so that the [optimizer](where-clause-the-equals-operator.md#slow-indexes-part-ii) knows a little bit about the table’s content.

#### Concatenated Keys

This script changes the `EMPLOYEES` table so that it reflects the situation after the merger with Very Big Company:

```
-- add subsidiary_id and update existing records
ALTER TABLE employees ADD subsidiary_id NUMERIC;
UPDATE      employees SET subsidiary_id = 30;
ALTER TABLE employees ALTER COLUMN subsidiary_id SET NOT NULL;

-- change the PK
ALTER TABLE employees DROP CONSTRAINT employees_pk;
ALTER TABLE employees ADD CONSTRAINT employees_pk
      PRIMARY KEY (employee_id, subsidiary_id);

-- generate more records (Very Big Company)
INSERT INTO employees (employee_id,  first_name,
                       last_name,    date_of_birth,
                       phone_number, subsidiary_id, junk)
SELECT GENERATE_SERIES
     , initcap(lower(random_string(2, 8)))
     , initcap(lower(random_string(2, 8)))
     , CURRENT_DATE - CAST(floor(random() * 365 * 10 + 40 * 365) AS NUMERIC) * INTERVAL '1 DAY'
     , CAST(floor(random() * 9000 + 1000) AS NUMERIC)
     , CAST(floor(random() * (generate_series)/9000*29) AS NUMERIC)
     , 'junk'
  FROM GENERATE_SERIES(1, 9000);

VACUUM ANALYZE employees;
```

Notes:

- The new primary key just extended by the `SUBSIDIARY_ID`; that is, the `EMPLOYEE_ID` remains in the first position.
- The new records are randomly assigned to the subsidiaries 1 through 29.
- The table and index are analyzed again to make the optimizer aware of the grown data volume.

The next script introduces the index on `SUBSIDIARY_ID` to support the query for all employees of one particular subsidiary:

```
CREATE INDEX emp_sub_id ON employees (subsidiary_id);
```

Although that gives decent performance, it’s better to use the index that supports the primary key:

```
-- use tmp index to support the PK
CREATE UNIQUE INDEX employee_pk_tmp
    ON employees (subsidiary_id, employee_id);

 ALTER TABLE employees
   ADD CONSTRAINT employees_pk_tmp
UNIQUE (subsidiary_id, employee_id);

ALTER TABLE employees
 DROP CONSTRAINT employees_pk;

ALTER TABLE employees
  ADD CONSTRAINT employees_pk
      PRIMARY KEY (subsidiary_id, employee_id);

ALTER TABLE employees
 DROP CONSTRAINT employees_pk_tmp;

-- drop old indexes
DROP INDEX employee_pk_tmp;
DROP INDEX emp_sub_id;
```

Notes:

- A new index is created and used to support the PK.
- Once the old PK index isn’t used by the constraint anymore, it can be dropped and recreated with its new column order.
- The constraint is changed again to use the new PK index and the temporary index can be dropped—as well as the index on the subsidiary id that isn’t required anymore.

### Functions

#### Case-Insensitive Search

The randomized names were already created in correct case, just update “my” record:

```
UPDATE employees
   SET first_name = 'Markus'
     , last_name  = 'Winand'
 WHERE employee_id   = 123
   AND subsidiary_id = 30;
```

The statement to create the function-based index:

```
CREATE INDEX emp_up_name
    ON employees (UPPER(last_name) varchar_pattern_ops);;
DROP INDEX emp_name;;
```

```
VACUUM ANALYZE employees;;
```

#### User-Defined Functions

Define a PL/SQL function that calculates the age and attempts to use it in an index:

```
CREATE FUNCTION get_age(date_of_birth DATE)
RETURNS NUMERIC
AS
$$
BEGIN
    RETURN DATE_PART('year', AGE(date_of_birth));
END;
$$ LANGUAGE plpgsql;

CREATE INDEX invalid ON EMPLOYEES (get_age(date_of_birth));
```

You should get the error “functions in index expression must be marked IMMUTABLE”.

### Partial Indexes

```
CREATE TABLE messages AS (
SELECT GENERATE_SERIES::numeric id
     , CASE WHEN random() < 0.01 THEN 'N' ELSE 'Y' END processed
     , CAST(trunc(random() * 100) AS NUMERIC) receiver
     , 'junk' message
  FROM GENERATE_SERIES(0, 999999)
);

CREATE INDEX messages_todo
          ON messages (receiver, processed);
```

```
PREPARE stmt(int) AS
 SELECT message
   FROM messages
  WHERE processed = 'N'
    AND receiver  = $1;

EXPLAIN EXECUTE stmt(1);
```

```
CREATE INDEX messages_only_todo
          ON messages (receiver)
       WHERE processed = 'N';

PREPARE stmt(int) AS
 SELECT message
   FROM messages
  WHERE processed = 'N'
    AND receiver  = $1;

EXPLAIN EXECUTE stmt(1);
```


## PostgreSQL Example Scripts for “Testing and Scalability”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/postgresql/performance-testing-scalability</sub>

This section contains the `CREATE`, `INSERT` and PL/pgSQL code to run the scalability test from the [Testing and Scalability Chapter](testing-scalability.md) in a PostgreSQL database.

> **Warning:**
>
> These scripts will create large objects in the database and produce a huge amount of transaction logs.

It’s required to run the test against a very large data set to make sure caching does not affect the measurement. Depending on your environment, you might need to create even larger tables to reproduce a linear result as shown in the book.

```
CREATE TABLE scale_data (
   section NUMERIC NOT NULL,
   id1     NUMERIC NOT NULL,
   id2     NUMERIC NOT NULL
);
```

Note:

- There is no primary key (to keep the data generation simple).
- There is no index (yet). That’s done after filling the table.
- There is no “junk” column to keep the table small.

```
INSERT INTO scale_data
SELECT sections.*, gen.*
     , CEIL(RANDOM()*100)
  FROM GENERATE_SERIES(1, 300)     sections,
       GENERATE_SERIES(1, 900000) gen
 WHERE gen <= sections * 3000;
```

Note:

- This code generates 300 sections, you may need to adjust the number for your environment. If you increase the number of sections, you might also need to increase second `GENERATE_SERIES` call. It must generate at least `3000 x <number of sections>` records.
- The table will need some gigabytes.

```
CREATE INDEX scale_slow ON scale_data (section, id1, id2);

ALTER TABLE scale_data CLUSTER ON scale_slow;
CLUSTER scale_data;
```

Note:

- The index will also need some gigabytes.
- PostgreSQL doesn’t support covering indexes as of release 9.0.3. That means, it’s not possible to select from an index only, without the corresponding table access. We will therefore cluster the table according to the index, to keep the impact at a minimum.
- That might take ages.

```
CREATE OR REPLACE FUNCTION test_scalability
   (sql_txt VARCHAR(2000), n INT)
   RETURNS SETOF RECORD AS
$$
DECLARE
   tim   INTERVAL[300];
   rec   INT[300];
   strt  TIMESTAMP;
   v_rec RECORD;
   iter  INT;
   sec   INT;
   cnt   INT;
   rnd   INT;
BEGIN
   FOR iter  IN 0..n LOOP
      FOR sec IN 0..300 LOOP
         IF iter = 0 THEN
           tim[sec] := 0;
           rec[sec] := 0;
         END IF;
         rnd  := CEIL(RANDOM() * 100);
         strt := CLOCK_TIMESTAMP();

         EXECUTE 'select count(*) from (' || sql_txt || ') tbl'
            INTO cnt
           USING sec, rnd;

         tim[sec] := tim[sec] + CLOCK_TIMESTAMP() - strt;
         rec[sec] := rec[sec] + cnt;

         IF iter = n THEN
            SELECT INTO v_rec sec, tim[sec], rec[sec];
            RETURN NEXT v_rec;
         END IF;
      END LOOP;
   END LOOP;

   RETURN;
END;
$$ LANGUAGE plpgsql;
```

Note:

- The `TEST_SCALABILITY` function returns a table.
- It’s hardcoded to run the test 300 sections
- The number of iterations is configurable

```
SELECT *
  FROM test_scalability('SELECT * '
                      ||  'FROM scale_data '
                      || 'WHERE section=$1 '
                      ||   'AND id2=$2', 10)
       AS (sec INT, seconds INTERVAL, cnt_rows INT);
```

The counter test, with a better index, can be done like that:

```
CREATE INDEX scale_fast ON scale_data (section, id2, id1);

ALTER TABLE scale_data CLUSTER ON scale_fast;
CLUSTER scale_data;

SELECT *
  FROM test_scalability('SELECT * '
                      ||  'FROM scale_data '
                      || 'WHERE section=$1 '
                      ||   'AND id2=$2', 10)
       AS (sec INT, seconds INTERVAL, cnt_rows INT);
```

Note:

- It’s required to cluster the table on the new index. That might take ages.


## PostgreSQL Example Scripts for “The Join Operation”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/postgresql/join</sub>

This section contains the `CREATE` and `INSERT` code to run the examples from “[The Join Operator](join.md)” in a PostgreSQL database.

```
CREATE TABLE sales (
  sale_id       NUMERIC NOT NULL,
  employee_id   NUMERIC NOT NULL,
  subsidiary_id NUMERIC NOT NULL,
  sale_date     DATE    NOT NULL,
  eur_value     NUMERIC(17,2) NOT NULL,
  product_id    BIGINT  NOT NULL,
  quantity      INTEGER NOT NULL,
  junk          CHAR(200),
  CONSTRAINT sales_pk
     PRIMARY KEY (sale_id),
  CONSTRAINT sales_emp_fk
     FOREIGN KEY          (subsidiary_id, employee_id)
      REFERENCES employees(subsidiary_id, employee_id)
);

SELECT SETSEED(0);

INSERT INTO sales (sale_id
                 , subsidiary_id, employee_id
                 , sale_date, eur_value
                 , product_id, quantity
                 , junk)
SELECT row_number() OVER (), data.*
  FROM (
       SELECT e.subsidiary_id, e.employee_id
            , (CURRENT_DATE - CAST(RANDOM()*3650 AS NUMERIC) * INTERVAL '1 DAY') sale_date
            , CAST(RANDOM()*100000 AS NUMERIC)/100 eur_value
            , CAST(RANDOM()*25 AS NUMERIC) + 1 product_id
            , CAST(RANDOM()*5 AS NUMERIC) + 1 quantity
            , 'junk'
         FROM employees e
            , GENERATE_SERIES(1, 1800) gen
        WHERE MOD(employee_id, 7) = 4
          AND gen < employee_id / 5
        ORDER BY sale_date
       ) data
 WHERE TO_CHAR(sale_date, 'D') <> '1';

VACUUM ANALYZE sales;
```

Notes:

- The rows are inserted chronologically to reflect a natural table growth.
- Only a small fraction of employees have sales at all.


## PostgreSQL Example Scripts for “Sorting and Grouping”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/postgresql/sorting-grouping</sub>

This section contains the code and execution plans for [Chapter 6*Sorting and Grouping*](sorting-grouping.md) in a PostgreSQL database.

### Indexed Order By

```
  DROP INDEX sales_date;
CREATE INDEX sales_dt_pr ON sales (sale_date, product_id);

EXPLAIN
 SELECT sale_date, product_id, quantity
   FROM sales
  WHERE sale_date = now() - INTERVAL '1' DAY
  ORDER BY sale_date, product_id;
```

The execution does not perform a sort operation:

```
                         QUERY PLAN
-----------------------------------------------------------
Index Scan using sales_dt_pr (cost=0.01..680.86 rows=376)
  Index Cond: (sale_date = (now() - '1 day'::interval day))
```

PostgreSQL uses the same execution plan, when sorting by `PRODUCT_ID` only.

```
EXPLAIN
 SELECT sale_date, product_id, quantity
   FROM sales
  WHERE sale_date = now() - INTERVAL '1' DAY
  ORDER BY product_id;
```

Using an greater or equals condition requires an Sort operation:

```
EXPLAIN
 SELECT sale_date, product_id, quantity
   FROM sales
  WHERE sale_date >= now() - INTERVAL '1' DAY
  ORDER BY product_id;
```

Although the row estimate got lower, causing the cost also to be lower:

```
                         QUERY PLAN
--------------------------------------------------------------
Sort  (cost=8.50..8.50 rows=1 width=32)
 Sort Key: product_id
 -> Index Scan using sales_dt_pr (cost=0.00..8.49 rows=1)
    Index Cond: (sale_date >= (now() - '1 day'::interval day))
```

### Indexing ASC, DESC and NULLS FIRST/LAST

Scanning an index backwards:

```
EXPLAIN
 SELECT sale_date, product_id, quantity
   FROM sales
  WHERE sale_date >= now() - INTERVAL '1' DAY
  ORDER BY sale_date DESC, product_id DESC;
```

Mixing `ASC` and `DESC` causes an explicit sort:

```
EXPLAIN
 SELECT sale_date, product_id, quantity
   FROM sales
  WHERE sale_date >= now() - INTERVAL '1' DAY
  ORDER BY sale_date ASC, product_id DESC;
```

Ordering the index with mixed `ASC`/`DESC` modifiers:

```
  DROP INDEX sales_dt_pr;

CREATE INDEX sales_dt_pr
    ON sales (sale_date ASC, product_id DESC);

EXPLAIN
 SELECT sale_date, product_id, quantity
   FROM sales
  WHERE sale_date >= now() - INTERVAL '1' DAY
  ORDER BY sale_date ASC, product_id DESC;
```

PostgreSQL orders `NULLS LAST` per default. However, the `DESC` modifier puts them to the front, so that `DESC NULLS LAST` must sort explicitly:

```
EXPLAIN
 SELECT sale_date, product_id, quantity
   FROM sales
  WHERE sale_date >= now() - INTERVAL '1' DAY
  ORDER BY sale_date ASC, product_id DESC NULLS LAST;
```

PostgreSQL allows explicit `NULLS LAST` indexing as well, so it becomes a pipelined `order by` again:

```
  DROP INDEX sales_dt_pr;

CREATE INDEX sales_dt_pr
    ON sales (sale_date ASC, product_id DESC NULLS LAST);

EXPLAIN
 SELECT sale_date, product_id, quantity
   FROM sales
  WHERE sale_date >= now() - INTERVAL '1' DAY
  ORDER BY sale_date ASC, product_id DESC NULLS LAST;
```

### Indexed Group By

The PostgreSQL (at least 9.0-13) database seems to have a little glitch, that makes the following not work as pipelined `order by`, when the index has `NULLS LAST`, as created above:

```
EXPLAIN
 SELECT product_id, SUM(eur_value)
   FROM sales
  WHERE sale_date = now() - INTERVAL '1' DAY
  GROUP BY product_id;
```

```
                                     QUERY PLAN
------------------------------------------------------------------------------------
 HashAggregate  (cost=574.21..574.53 rows=26 width=40)
   Group Key: product_id
   ->  Index Scan using sales_dt_pr on sales  (cost=0.43..572.62 rows=318 width=14)
         Index Cond: (sale_date = (now() - '1 day'::interval day))
```

Removing the `NULLS LAST` clause from the index reveals something interesting:

```
  DROP INDEX sales_dt_pr;

CREATE INDEX sales_dt_pr
    ON sales (sale_date ASC, product_id DESC);

EXPLAIN
 SELECT product_id, SUM(eur_value)
   FROM sales
  WHERE sale_date = now() - INTERVAL '1' DAY
  GROUP BY product_id;
```

```
                                         QUERY PLAN
---------------------------------------------------------------------------------------------
 GroupAggregate  (cost=0.43..574.53 rows=26 width=40)
   Group Key: product_id
   ->  Index Scan Backward using sales_dt_pr on sales  (cost=0.43..572.62 rows=318 width=14)
         Index Cond: (sale_date = (now() - '1 day'::interval day))
```

The index is read backwards, although the SQL statement does not require it. It seems like PostgreSQL uses an `GROUP BY PRODUCT_ID` internally, to fetch the pre-sorted result. The next case adds an `order by` clause in opposite order.

```
EXPLAIN
 SELECT product_id, SUM(eur_value)
   FROM sales
  WHERE sale_date = now() - INTERVAL '1' DAY
  GROUP BY product_id
  ORDER BY product_id DESC;
```

```
                                     QUERY PLAN
------------------------------------------------------------------------------------
 GroupAggregate  (cost=0.43..574.53 rows=26 width=40)
   Group Key: product_id
   ->  Index Scan using sales_dt_pr on sales  (cost=0.43..572.62 rows=318 width=14)
         Index Cond: (sale_date = (now() - '1 day'::interval day))
```

So, it seems that the implicit `order by` is not hardcoded. Our next test show that PostgreSQL can actually use such an index, but only if an explicit `order by` clause asks for the same order:

```
  DROP INDEX sales_dt_pr;

CREATE INDEX sales_dt_pr
    ON sales (sale_date ASC, product_id DESC NULLS LAST);

EXPLAIN
 SELECT product_id, SUM(eur_value)
   FROM sales
  WHERE sale_date = now() - INTERVAL '1' DAY
  GROUP BY product_id
  ORDER BY product_id DESC NULLS LAST;
```

```
                                     QUERY PLAN
------------------------------------------------------------------------------------
 GroupAggregate  (cost=0.43..574.53 rows=26 width=40)
   Group Key: product_id
   ->  Index Scan using sales_dt_pr on sales  (cost=0.43..572.62 rows=318 width=14)
         Index Cond: (sale_date = (now() - '1 day'::interval day))
```

It does a pipelined `group by`, even when the index is defined `NULLS LAST` when the order by clause explicitly sorts the same way. Otherwise, it uses an internal order by clause that disregards the indexes `NULLS` specification.

Further testing shows that the problem exists for `ASC` indexes with `NULLS FIRST`:

```
  DROP INDEX sales_dt_pr;

CREATE INDEX sales_dt_pr
    ON sales (sale_date ASC, product_id ASC NULLS FIRST);

EXPLAIN
 SELECT product_id, SUM(eur_value)
   FROM sales
  WHERE sale_date = now() - INTERVAL '1' DAY
  GROUP BY product_id
  ORDER BY product_id ASC NULLS FIRST;
```

```
                                     QUERY PLAN
------------------------------------------------------------------------------------
 GroupAggregate  (cost=0.43..574.53 rows=26 width=40)
   Group Key: product_id
   ->  Index Scan using sales_dt_pr on sales  (cost=0.43..572.62 rows=318 width=14)
         Index Cond: (sale_date = (now() - '1 day'::interval day))
```

But skipping `order b`y clause:

```
EXPLAIN
 SELECT product_id, SUM(eur_value)
   FROM sales
  WHERE sale_date = now() - INTERVAL '1' DAY
  GROUP BY product_id;
```

```
                                     QUERY PLAN
------------------------------------------------------------------------------------
 HashAggregate  (cost=574.21..574.53 rows=26 width=40)
   Group Key: product_id
   ->  Index Scan using sales_dt_pr on sales  (cost=0.43..572.62 rows=318 width=14)
         Index Cond: (sale_date = (now() - '1 day'::interval day))
```


## PostgreSQL Example Scripts for “Partial Results”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/postgresql/partial-results</sub>

This section contains the `create` and `insert` statements to run the examples from [Chapter 7*Partial Results*](partial-results.md) in a PostgreSQL database.

The test approach for the scalability of Top-N queries is the same as used in the “[Testing and Scalability](#postgresql-example-scripts-for-testing-and-scalability)” chapter.

### Querying Top-N Rows

First, using an index for the `where` clause only:

```
DROP INDEX scale_fast;
CREATE INDEX scale_slow ON scale_data (SECTION, ID1, ID2);
ALTER TABLE scale_data CLUSTER ON scale_slow;
CLUSTER scale_data;

SELECT *
  FROM test_scalability('SELECT * '
                      ||  'FROM scale_data '
                      || 'WHERE section=$1 '
                      || 'ORDER BY id2, id1 '
                      || 'FETCH FIRST 100 ROWS ONLY', 10)
       AS (sec INT, seconds INTERVAL, cnt_rows INT);
```

Then, using a pipelined Top-N with an index covering the `order by` clause:

```
CREATE INDEX scale_fast ON scale_data (SECTION, ID2, ID1);
ALTER TABLE scale_data CLUSTER ON scale_fast;
CLUSTER scale_data;

SELECT *
  FROM test_scalability('SELECT * '
                      ||  'FROM scale_data '
                      || 'WHERE section=$1 '
                      || 'ORDER BY id2, id1 '
                      || 'FETCH FIRST 100 ROWS ONLY', 10)
       AS (sec INT, seconds INTERVAL, cnt_rows INT);
```

### Paging Through Results

The following function uses both methods to fetch the result page-wise. The select statement in the end prepares the statistics on screen.

```
CREATE OR REPLACE
FUNCTION test_topn_scalability (n INT)
 RETURNS SETOF RECORD AS
$$
DECLARE
  strt  TIMESTAMP;
  dur   INTERVAL;
  v_rec RECORD;
  mode  INT; iter  INT; sec   INT;
  lf    RECORD;
  c1    INT[300]; c2 INT[300];

  sql_restart CURSOR (sec int, page int)
           IS SELECT id2, id1
                FROM scale_data
               WHERE section = sec
               ORDER BY id2,id1
              OFFSET 100*page
               FETCH NEXT 100 ROWS ONLY;

  sql_continue CURSOR (sec int, c2 int, c1 int)
            IS SELECT id2, id1
                 FROM scale_data
                WHERE section = sec
              --    AND (id2, id1) > (c2, c1)
                  AND id2 >= c2
                  AND (
                         (id2 = c2 AND id1 > c1)
                       OR
                         (id2 > c2)
                      )
                ORDER BY id2,id1
                FETCH NEXT 100 ROWS ONLY;
BEGIN
  FOR iter  IN 1..n LOOP
    FOR mode  IN 0..1 LOOP
      FOR page IN 0..100 LOOP
        FOR sec IN 0..300 LOOP
          strt := CLOCK_TIMESTAMP();

          IF mode = 0 or page = 0 THEN
            FOR lf IN sql_restart(sec, page) LOOP
              c1[sec] := lf.id1; c2[sec] := lf.id2;
            END LOOP;
          ELSE
            FOR lf IN sql_continue(sec, c2[sec], c1[sec]) LOOP
              c1[sec] := lf.id1; c2[sec] := lf.id2;
            END LOOP;
          END IF;

          dur := (CLOCK_TIMESTAMP() - strt);

          SELECT INTO v_rec mode, sec, page, dur;
          RETURN NEXT v_rec;
        END LOOP;
      END LOOP;
    END LOOP;
  END LOOP;
  RETURN;
END;
$$ LANGUAGE plpgsql;

SELECT sec, mode, page, sum(seconds)
  FROM test_topn_scalability(10)
    AS (mode INT, sec INT, page int, seconds INTERVAL)
 WHERE sec=10
 GROUP BY sec, mode, page
 ORDER BY sec, mode, page;
```

### Window-Functions

PostgreSQL supports window-Functions, but does, as of release 9.1, not use indexes for the best benefits.

```
SELECT *
  FROM ( SELECT sales.*
              , ROW_NUMBER() OVER (ORDER BY sale_date DESC
                                          , sale_id   DESC) rn
           FROM sales
       ) tmp
 WHERE rn between 11 and 20
 ORDER BY sale_date DESC, sale_id DESC
```

```
                          QUERY PLAN
---------------------------------------------------------------
Subquery Scan on tmp  (cost=606750.08..649178.30 rows=6061)
 Filter: ((tmp.rn >= 11) AND (tmp.rn <= 20))
 -> WindowAgg  (cost=606750.08..630994.78 rows=1212235)
    -> Sort  (cost=606750.08..609780.67 rows=1212235)
       Sort Key: sales.sale_date, sales.sale_id
       -> Seq Scan on sales  (cost=0.00..55417.35 rows=1212235)
```

The database reads the entire table (`Seq Scan`) and sorts it (`Sort`). There is no `Limit` operation that would indicate awareness of the statements purpose.


## PostgreSQL Example Scripts for “Insert, Delete and Update”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/postgresql/dml</sub>

This section contains the `create` and `insert` statements to run the examples from [Chapter 8*Modifying Data*](dml.md) in an PostgreSQL database. There is only one query that reports all figures for the `insert`, `delete` and `update` sections.

```
CREATE TABLE scale_write_0 AS (
SELECT GENERATE_SERIES::numeric id1
     , (random() * 9000000)::numeric + 10000000 id2
     , (random() * 9000000)::numeric + 10000000 id3
     , (random() * 9000000)::numeric + 10000000 id4
     , (random() * 9000000)::numeric + 10000000 id5
  FROM GENERATE_SERIES(10000000, 19999999)
);

CREATE TABLE scale_write_1
AS (SELECT * from scale_write_0);

CREATE TABLE scale_write_2
AS (SELECT * from scale_write_0);

CREATE TABLE scale_write_3
AS (SELECT * from scale_write_0);

CREATE TABLE scale_write_4
AS (SELECT * from scale_write_0);

CREATE TABLE scale_write_5
AS SELECT * from scale_write_0;

CREATE INDEX scale_write_1_1 on scale_write_1(id1);

CREATE INDEX scale_write_2_1 on scale_write_2(id1);
CREATE INDEX scale_write_2_2 on scale_write_2(id2, id1);

CREATE INDEX scale_write_3_1 on scale_write_3(id1);
CREATE INDEX scale_write_3_2 on scale_write_3(id2, id1);
CREATE INDEX scale_write_3_3 on scale_write_3(id3, id2, id1);

CREATE INDEX scale_write_4_1 on scale_write_4(id1);
CREATE INDEX scale_write_4_2 on scale_write_4(id2, id1);
CREATE INDEX scale_write_4_3 on scale_write_4(id3, id2, id1);
CREATE INDEX scale_write_4_4 on scale_write_4(id4, id3, id2
                                             ,id1);

CREATE INDEX scale_write_5_1 on scale_write_5(id1);
CREATE INDEX scale_write_5_2 on scale_write_5(id2, id1);
CREATE INDEX scale_write_5_3 on scale_write_5(id3, id2, id1);
CREATE INDEX scale_write_5_4 on scale_write_5(id4, id3, id2
                                             ,id1);
CREATE INDEX scale_write_5_5 on scale_write_5(id5, id4, id3
                                             ,id2, id1);
```

```
CREATE OR REPLACE
FUNCTION run_insert(idxes INT, lb INT, ub INT, n INT)
 RETURNS VARCHAR AS
$$
DECLARE
  rows_affected INT;
  r2 INT;
  r3 INT;
  r4 INT;
  r5 INT;
  d1 INT;
BEGIN
  WHILE n > 0 LOOP
    d1 := (random() * (ub-lb))::INT + lb;
    r2 := (random() * 9000000)::INT;
    r3 := (random() * 9000000)::INT;
    r4 := (random() * 9000000)::INT;
    r5 := (random() * 9000000)::INT;
    CASE idxes
    WHEN 0 THEN
           INSERT INTO scale_write_0 (id1, id2, id3, id4, id5)
                              VALUES ( d1,  r2,  r3,  r4,  r5);
    WHEN 1 THEN
           INSERT INTO scale_write_1 (id1, id2, id3, id4, id5)
                              VALUES ( d1,  r2,  r3,  r4,  r5);
    WHEN 2 THEN
           INSERT INTO scale_write_2 (id1, id2, id3, id4, id5)
                              VALUES ( d1,  r2,  r3,  r4,  r5);
    WHEN 3 THEN
           INSERT INTO scale_write_3 (id1, id2, id3, id4, id5)
                              VALUES ( d1,  r2,  r3,  r4,  r5);
    WHEN 4 THEN
           INSERT INTO scale_write_4 (id1, id2, id3, id4, id5)
                              VALUES ( d1,  r2,  r3,  r4,  r5);
    WHEN 5 THEN
           INSERT INTO scale_write_5 (id1, id2, id3, id4, id5)
                              VALUES ( d1,  r2,  r3,  r4,  r5);
    END CASE;
    n := n - 1;
  END LOOP;
  RETURN 'insert';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE
FUNCTION run_delete(tbl INT, lb INT, ub INT, n INT)
 RETURNS VARCHAR AS
$$
DECLARE
  rows_affected INT;
  aff  INT := 0;
  d1   INT;
  iter INT := n;
BEGIN
  WHILE iter > 0 LOOP
    d1 := (random() * (ub-lb))::INT + lb;
    CASE tbl
    WHEN 1 THEN
           DELETE FROM scale_write_1 WHERE id1 = d1;
    WHEN 2 THEN
           DELETE FROM scale_write_2 WHERE id1 = d1;
    WHEN 3 THEN
           DELETE FROM scale_write_3 WHERE id1 = d1;
    WHEN 4 THEN
           DELETE FROM scale_write_4 WHERE id1 = d1;
    WHEN 5 THEN
           DELETE FROM scale_write_5 WHERE id1 = d1;
    ELSE NULL;
    END CASE;
    iter := iter - 1;
    GET DIAGNOSTICS rows_affected = ROW_COUNT;
    aff := aff + rows_affected;
  END LOOP;
  RETURN CASE WHEN aff = n THEN 'delete'
         ELSE NULL END;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE
FUNCTION run_update_all(tbl INT, lb INT, ub INT, n INT)
RETURNS VARCHAR AS
$$
DECLARE
  rows_affected INT;
  r2  INT;
  r3  INT;
  r4  INT;
  r5  INT;
  d1  INT;
  iter INT := n;
  aff  INT := 0;
BEGIN
  WHILE iter > 0 LOOP
    d1 := (random() * (ub-lb))::INT + lb;
    r2 := (random() * 9000000)::INT;
    r3 := (random() * 9000000)::INT;
    r4 := (random() * 9000000)::INT;
    r5 := (random() * 9000000)::INT;
    CASE tbl
    WHEN 1 THEN
           UPDATE scale_write_1
              SET id2 = r2, id3=r3, id4=r4, id5=r5 WHERE id1=d1;
    WHEN 2 THEN
           UPDATE scale_write_2
              SET id2 = r2, id3=r3, id4=r4, id5=r5 WHERE id1=d1;
    WHEN 3 THEN
           UPDATE scale_write_3
              SET id2 = r2, id3=r3, id4=r4, id5=r5 WHERE id1=d1;
    WHEN 4 THEN
           UPDATE scale_write_4
              SET id2 = r2, id3=r3, id4=r4, id5=r5 WHERE id1=d1;
    WHEN 5 THEN
           UPDATE scale_write_5
              SET id2 = r2, id3=r3, id4=r4, id5=r5 WHERE id1=d1;
    ELSE NULL;
    END CASE;
    iter := iter - 1;
    GET DIAGNOSTICS rows_affected = ROW_COUNT;
    aff := aff + rows_affected;
  END LOOP;
  RETURN CASE WHEN aff = n THEN 'update all'
         ELSE NULL END;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE
FUNCTION run_update_one(tbl INT, lb INT, ub INT, n INT)
RETURNS VARCHAR AS
$$
DECLARE
  rows_affected INT;
  r  INT;
  d1 INT;
  aff  INT := 0;
  iter INT := n;
BEGIN
  WHILE iter > 0 LOOP
    d1 := (random() * (ub-lb))::INT + lb;
    r  := (random() * 9000000)::INT;
    CASE tbl
    WHEN 1 THEN -- no index updated
           UPDATE scale_write_1 SET id2 = r WHERE id1=d1;
    WHEN 2 THEN -- one index updated
           UPDATE scale_write_2 SET id2 = r WHERE id1=d1;
    WHEN 3 THEN -- one index updated
           UPDATE scale_write_3 SET id3 = r WHERE id1=d1;
    WHEN 4 THEN -- one index updated
           UPDATE scale_write_4 SET id4 = r WHERE id1=d1;
    WHEN 5 THEN -- one index updated
           UPDATE scale_write_5 SET id5 = r WHERE id1=d1;
    ELSE NULL;
    END CASE;
    iter := iter - 1;
    GET DIAGNOSTICS rows_affected = ROW_COUNT;
    aff := aff + rows_affected;
  END LOOP;
  RETURN CASE WHEN aff = n THEN 'update one'
         ELSE NULL END;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE
FUNCTION test_write_scalability (n INT)
 RETURNS SETOF RECORD AS
$$
DECLARE
  rec  RECORD;
  strt TIMESTAMP;
  mode VARCHAR;
  cmnd INT;
  q    INT;
  lb   INT;
  alb  INT;
  iter INT;
  idxs INT;
BEGIN
  SELECT ((max(id1)-min(id1))/4)::INT, min(id1)::INT
    INTO q, alb
    FROM scale_write_1;

  FOR iter IN 1 .. n LOOP
    FOR cmnd IN 0 .. 3 LOOP
      FOR idxs IN 0 .. 5 LOOP
        lb   := alb + cmnd*q;
        strt := CLOCK_TIMESTAMP();
        mode :=
          CASE cmnd
          WHEN 0 THEN run_insert    (idxs, lb, lb+q, 1)
          WHEN 1 THEN run_update_one(idxs, lb, lb+q, 1)
          WHEN 2 THEN run_delete    (idxs, lb, lb+q, 1)
          WHEN 3 THEN run_update_all(idxs, lb, lb+q, 1)
          END;

        IF mode IS NOT NULL THEN
           SELECT INTO rec
                  idxs, mode, (CLOCK_TIMESTAMP() - strt);
           RETURN NEXT rec;
        END IF;
      END LOOP;
    END LOOP;
  END LOOP;
  RETURN;
END;
$$ LANGUAGE plpgsql;
```

```
SELECT indxes
     , mode
     , AVG(seconds)  seconds
     , TO_CHAR (STDDEV(EXTRACT(epoch FROM seconds))
                / AVG(EXTRACT(epoch FROM seconds))
                * 100
               , '999.9') std_dev_prc
  FROM test_write_scalability(10)
    AS (indxes INT, mode VARCHAR, seconds INTERVAL)
 GROUP BY indxes, mode
 ORDER BY mode, indxes;
```


## PostgreSQL Example Scripts for “3-Minute Quiz”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/postgresql/3-minute-quiz</sub>

This section contains the `create`, `insert` and `select` statements for the “[Test your SQL Know-How in 3 Minutes](https://use-the-index-luke.com/3-minute-quiz)” test. You may want to test yourself before reading this page.

The `create` and `insert` statements are available in the [example schema archive](https://use-the-index-luke.com/use-the-index-luke.tar.gz).

The execution plans shown are abbreviated for better readability.

### Question 1 — DATE Anti-Pattern

```
CREATE INDEX tbl_idx ON tbl (date_column);
```

```
SELECT COUNT(*)
  FROM tbl
 WHERE EXTRACT(YEAR FROM date_column) = 2025;
```

```
SELECT COUNT(*)
  FROM tbl
 WHERE date_column >= DATE'2025-01-01'
   AND date_column <  DATE'2026-01-01';
```

The first execution plan performs a full table scan (`Seq Scan`). The second execution plan, on the other hand, uses the index.

```
                    QUERY PLAN
----------------------------------------------------
Aggregate (actual rows=1 loops=1)
Buffers: shared hit=3
-> Seq Scan on tbl (actual rows=271 loops=1)
   Filter: (date_part('year', date_column) = '2017')
   Rows Removed by Filter: 29
   Buffers: shared hit=3
```

```
                  QUERY PLAN
------------------------------------------------
Aggregate (actual rows=1 loops=1)
Buffers: shared hit=3
-> Seq Scan on tbl (actual rows=271 loops=1)
   Filter: ((date_column >= '2017-01-01'::date)
        AND (date_column <  '2018-01-01'::date))
   Rows Removed by Filter: 29
   Buffers: shared hit=3
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
 FETCH FIRST 1 ROW ONLY;
```

The query uses the index (`Index Scan`) and fetches in reverse order (`Backward`). Note that there is no sort operation.

```
                            QUERY PLAN
-------------------------------------------------------------------
Limit (actual rows=1 loops=1)
Buffers: shared hit=3
-> Index Scan Backward using tbl_idx on tbl (actual rows=1 loops=1)
   Index Cond: (a = '12'::numeric)
   Buffers: shared hit=3
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
                       QUERY PLAN
--------------------------------------------------------
Index Scan using tbl_idx on tbl (actual rows=1 loops=1)
Index Cond: ((a = '38'::numeric) AND (b = '1'::numeric))
Buffers: shared hit=3
```

```
                       QUERY PLAN
--------------------------------------------------------
Index Scan using tbl_idx on tbl (actual rows=1 loops=1)
Index Cond: ((b = '1'::numeric) AND (a = '38'::numeric))
Buffers: shared hit=3
```

The second query reads the entire table (`Seq Scan`). Changing the column order in the index allows both queries to use a `(Bitmap) Index Scan`.

```
              QUERY PLAN
---------------------------------------
Seq Scan on tbl (actual rows=2 loops=1)
Filter: (b = '1'::numeric)
Rows Removed by Filter: 298
Buffers: shared hit=11
```

```
                      QUERY PLAN
-------------------------------------------------------
Index Scan using tbl_idx on tbl (actual rows=2 loops=1)
Index Cond: (b = '1'::numeric)
Buffers: shared hit=4
```

> **Learn More:**
>
> - [The column order in Multi-Column Indexes](where-clause-the-equals-operator.md#concatenated-indexes)
> - [PostgreSQL operations Index Scan and Bitmap Index Scan](explain-plan-postgresql.md#operations)

### Question 4 — LIKE

```
CREATE INDEX tbl_idx ON tbl (text varchar_pattern_ops);
```

```
SELECT *
  FROM tbl
 WHERE text LIKE 'TJ%';
```

The execution plan states that it is doing an *Index Scan*. Since there is only wild card character at the very end, the full search text `'TJ'` can be used as [index access predicate](where-clause-searching-for-ranges.md#greater-less-and-between).

The [PostgreSQL execution plan does not immediately reveal whether the index access is a full index scan or an index range scan](explain-plan-postgresql.md#distinguishing-access-and-filter-predicates). To be safe, you can increase the table size (like 100-fold) and check the number of processed blocks for this query (`explain (analyse, buffers)`).

```
                      QUERY PLAN
-------------------------------------------------------
Index Scan using tbl_idx on tbl (actual rows=1 loops=1)
Index Cond: (((text)::text ~>=~ 'TJ'::text)
         AND ((text)::text ~<~ 'TK'::text))
Filter: ((text)::text ~~ 'TJ%'::text)
Buffers: shared hit=3
```

> **Learn More:**
>
> - [A visual explanation why LIKE is slow](where-clause-searching-for-ranges.md#indexing-like-filters)
> - [PostgreSQL 9.1 Trigram Indexes for anywhere LIKE](https://www.postgresql.org/docs/current/pgtrgm.html)

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

Although the index is used efficiently in both queries, the first query does perform an [*Index Only Scan*](clustering.md#index-only-scan-avoiding-table-access). That means the second query performs less efficient as the first.

```
                          QUERY PLAN
---------------------------------------------------------------
GroupAggregate (actual rows=3 loops=1)
Group Key: date_column
Buffers: shared hit=3
-> Index Only Scan using tbl_idx on tbl (actual rows=3 loops=1)
   Index Cond: (a = '12'::numeric)
   Heap Fetches: 0
   Buffers: shared hit=3
```

```
                         QUERY PLAN
-------------------------------------------------------------
GroupAggregate (actual rows=1 loops=1)
Group Key: date_column
Buffers: shared hit=5
-> Sort (actual rows=1 loops=1)
   Sort Key: date_column
   Sort Method: quicksort  Memory: 25kB
   Buffers: shared hit=5
   -> Bitmap Heap Scan on tbl (actual rows=1 loops=1)
      Recheck Cond: (a = '38'::numeric)
      Filter: (b = '1'::numeric)
      Rows Removed by Filter: 2
      Heap Blocks: exact=3
      Buffers: shared hit=5
      -> Bitmap Index Scan on tbl_idx (actual rows=3 loops=1)
         Index Cond: (a = '38'::numeric)
         Buffers: shared hit=2
```

> **Tip:**
>
> - [Chapter 5*Clustering Data: The Second Power of Indexing*](clustering.md)
> - [Reading MySQL explain plan output](explain-plan-mysql.md#operations)
