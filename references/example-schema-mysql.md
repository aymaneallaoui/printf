<!-- Source: https://use-the-index-luke.com/sql/example-schema/mysql — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# MySQL Example Scripts

<sub>Source: https://use-the-index-luke.com/sql/example-schema/mysql</sub>

The scripts provided in this appendix are ready to run and were tested on MySQL 5.1.

## Contents

1. *[The `where` clause](#mysql-example-scripts-for-the-where-clause)*
2. *[The Join Operation](#mysql-example-scripts-for-the-join-operation)*
3. *[Clustering Data](#mysql-example-scripts-for-clustering-data)*
4. *[Sorting and Grouping](#mysql-example-scripts-for-sorting-and-grouping)*
5. *[Partial Results](#mysql-example-scripts-for-partial-results)*
6. *[3-Minute Test](#mysql-example-scripts-for-3-minute-quiz)*


## MySQL Example Scripts for “The Where Clause”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/mysql/where-clause</sub>

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
CREATE OR REPLACE VIEW generator_16
AS SELECT 0 n UNION ALL SELECT 1  UNION ALL SELECT 2  UNION ALL
   SELECT 3   UNION ALL SELECT 4  UNION ALL SELECT 5  UNION ALL
   SELECT 6   UNION ALL SELECT 7  UNION ALL SELECT 8  UNION ALL
   SELECT 9   UNION ALL SELECT 10 UNION ALL SELECT 11 UNION ALL
   SELECT 12  UNION ALL SELECT 13 UNION ALL SELECT 14 UNION ALL
   SELECT 15;
```

```
CREATE OR REPLACE VIEW generator_256
AS SELECT ( ( hi.n << 4 ) | lo.n ) AS n
     FROM generator_16 lo, generator_16 hi;
```

```
CREATE OR REPLACE VIEW generator_4k
AS SELECT ( ( hi.n << 8 ) | lo.n ) AS n
     FROM generator_256 lo, generator_16 hi;
```

```
CREATE OR REPLACE VIEW generator_64k
AS SELECT ( ( hi.n << 8 ) | lo.n ) AS n
     FROM generator_256 lo, generator_256 hi;
```

```
INSERT INTO employees (employee_id,  first_name,
                       last_name,    date_of_birth,
                       phone_number, junk)
SELECT gen.n +1,
       GROUP_CONCAT(CHAR((RAND() * 25)+97) SEPARATOR ''),
       GROUP_CONCAT(CHAR((RAND() * 25)+97) SEPARATOR ''),
       SUBDATE(CURDATE(), INTERVAL (RAND()*3650 + 40*365) DAY),
       FLOOR(RAND()*9000+1000),
       'junk'
  FROM generator_4k gen, generator_16 rand
 WHERE gen.n < 1000
 GROUP BY gen.n;
```

```
UPDATE employees
   SET first_name='MARKUS',
       last_name='WINAND'
 WHERE employee_id=123;
```

```
ANALYZE TABLE employees;
```

Note:

- The `GENERATOR_X` views are row generators as described in the [MySQL Row Generator](https://use-the-index-luke.com/blog/2011-07-30/mysql-row-generator) article.
- The `JUNK` column is used to have a realistic row length. Because it’s data type is `CHAR`, as opposed to `VARCHAR`, it always stores 255 characters. Without this column the table would become unrealistically small and some demonstrations would not work.
- Random data is filled into the table, with exception to my entry, that is updated after the insert.
- Table and index [statistics](where-clause-the-equals-operator.md#slow-indexes-part-ii) are gathered so that the [optimizer](where-clause-the-equals-operator.md#slow-indexes-part-ii) knows a little bit about the table’s content.

#### Concatenated Keys

```
-- add subsidiary_id and update existing records
ALTER TABLE employees ADD subsidiary_id NUMERIC;
```

```
UPDATE      employees SET subsidiary_id = 30;
```

```
ALTER TABLE employees MODIFY subsidiary_id NUMERIC NOT NULL;
```

```
-- change the PK
ALTER TABLE employees DROP PRIMARY KEY;
```

```
ALTER TABLE employees ADD CONSTRAINT employees_pk
      PRIMARY KEY (employee_id, subsidiary_id);
```

```
-- generate more records (Very Big Company)
INSERT INTO employees (employee_id,  first_name,
                       last_name,    date_of_birth,
                       phone_number, subsidiary_id, junk)
SELECT gen.n + 1
     , GROUP_CONCAT(CHAR( RAND()*25 + 97) SEPARATOR '')
     , GROUP_CONCAT(CHAR( RAND()*25 + 97) SEPARATOR '')
     , CURDATE() - INTERVAL (RAND(0)*365*10 + 40*365) DAY
     , FLOOR(RAND()*9000 + 1000)
     , FLOOR(RAND()*(gen.n/9000)*29 + 1)
     , 'junk'
  FROM generator_64k gen, generator_16 rand
 WHERE gen.n < 9000
 GROUP BY gen.n;
```

```
ANALYZE TABLE employees;
```

Notes:

- The new primary key includes by the `SUBSIDIARY_ID`; that is, the `EMPLOYEE_ID` remains in the first position.
- The new records are randomly assigned to the subsidiaries 1 through 29.
- The table and index are analyzed again to make the optimizer aware of the grown data volume.

The next script introduces the index on `SUBSIDIARY_ID` to support the query for all employees of one particular subsidiary:

```
ALTER TABLE employees ADD INDEX emp_sub_id (subsidiary_id)
```

Although that gives decent performance, it’s better to use the index that supports the primary key:

```
-- use tmp index to support the PK
ALTER TABLE employees
  ADD UNIQUE INDEX tmp (employee_id, subsidiary_id);
```

```
ALTER TABLE employees
 DROP PRIMARY KEY;
```

```
ALTER TABLE employees
  ADD PRIMARY KEY (subsidiary_id, employee_id);
```

```
ALTER TABLE employees
 DROP INDEX tmp;
```

```
ALTER TABLE employees
 DROP INDEX emp_sub_id;
```

```
ANALYZE TABLE employees;
```

Notes:

- A new unique index is created and used to substitute the PK.
- The primary key is dropped and re-created with the new column order.
- The temporary index is dropped, as well as the index on the subsidiary id that isn’t required anymore.

### Functions

MySQL uses a case-insensitive collation per default. Further, MySQL did not support function-based indexes prior version 5.7. In that case, you don’t need a function-based index, a regular one will do as well:

```
CREATE INDEX emp_name ON employees (last_name)
```

Starting with 5.7, computed columns can be indexed in MySQL:

User-defined functions cannot be used in generated columns—not even when they are deterministic and declared deterministic.

```
CREATE FUNCTION get_age(date_of_birth DATE)
RETURNS INTEGER NO SQL
RETURN TIMESTAMPDIFF(YEAR,date_of_birth,CURDATE());
```

```
ALTER TABLE employees
  ADD COLUMN last_name_up VARCHAR(255) AS (UPPER(last_name));
```

```
CREATE INDEX emp_up_name ON employees (last_name_up);
```


## MySQL Example Scripts for “The Join Operation”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/mysql/join</sub>

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

INSERT INTO sales (sale_id
                 , subsidiary_id, employee_id
                 , sale_date, eur_value
                 , product_id, quantity
                 , junk)
SELECT @row := @row + 1 sale_id
     , data.*
  FROM (
       SELECT e.subsidiary_id, e.employee_id
            , CURDATE() - INTERVAL (RAND(0)*3650) DAY sale_date
            , TRUNCATE(RAND(1)*99.90+0.1, 2) eur_value
            , TRUNCATE(RAND(2)*25+1, 0) product_id
            , TRUNCATE(RAND(3)*5+1, 0) quantity
            , 'junk'
         FROM employees e
            , ( SELECT generator_4k.n+1 n
                  FROM generator_4k
                 WHERE generator_4k.n < 1800
              ) gen
        WHERE MOD(employee_id, 7) = 4
          AND gen.n < employee_id / 5
        ORDER BY sale_date
       ) data, (SELECT @row := 0) init
  WHERE DAYOFWEEK(sale_date) NOT IN (1,7);
```

Notes:

- The rows are inserted chronologically to reflect a natural table growth.
- Only a small fraction of employees have sales at all.


## MySQL Example Scripts for “Clustering Data”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/mysql/clustering-data</sub>

This section contains the `create` and `insert` code to run the examples from [Chapter 5*Clustering Data: The Second Power of Indexing*](clustering.md) in an MySQL database.

### Index-Organized Tables (Clustered Indexes)

The following creates a second `SALES` Table using the InnoDB engine so that it is created as clustered index. A secondary index is added on the `SALE_DATE` column.

```
CREATE TABLE sales_inno (
  sale_id       NUMERIC NOT NULL,
  employee_id   NUMERIC NOT NULL,
  subsidiary_id NUMERIC NOT NULL,
  sale_date     DATE   NOT NULL,
  eur_value     NUMERIC(17,2) NOT NULL,
  junk          CHAR(200),
  CONSTRAINT sales_pk
     PRIMARY KEY (sale_id)
) Engine=InnoDB;

INSERT INTO sales_inno (sale_id
                      , subsidiary_id, employee_id
                      , sale_date, eur_value, junk)
SELECT @row := @row + 1 sale_id
     , data.*
  FROM (
       SELECT e.subsidiary_id, e.employee_id
            , CURDATE() - INTERVAL (RAND(0)*3650) DAY sale_date
            , TRUNCATE(RAND(1)*99.90+0.1,2) eur_value
            , 'junk'
         FROM employees e
            , ( SELECT generator_4k.n+1 n
                  FROM generator_4k
                 WHERE generator_4k.n < 1800
              ) gen
        WHERE MOD(employee_id, 7) = 4
          AND gen.n < employee_id / 5
        ORDER BY sale_date
       ) data, (SELECT @row := 0) init
  WHERE DAYOFWEEK(sale_date) NOT IN (1,7);

CREATE INDEX sales_inno_dt ON sales_inno (sale_date);
```

Note the “Using Index” indicating an Index-Only Scan:

```
EXPLAIN
 SELECT sale_id
   FROM sales_inno
  WHERE sale_date = ?;

+----+------------+------+---------------+------+-------------+
| id | table      | type | key           | rows | Extra       |
+----+------------+------+---------------+------+-------------+
|  1 | sales_inno | ref  | sales_inno_dt |  301 | Using index |
+----+------------+------+---------------+------+-------------+
```


## MySQL Example Scripts for “Sorting and Grouping”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/mysql/sorting-grouping</sub>

This section contains the code and execution plans for [Chapter 6*Sorting and Grouping*](sorting-grouping.md) in a MySQL database.

### Indexed Order By

```
ALTER TABLE sales
 DROP INDEX sales_date;

ALTER TABLE sales
  ADD INDEX sales_dt_pr (sale_date, product_id);

EXPLAIN
 SELECT sale_date, product_id, quantity
   FROM sales
  WHERE sale_date = CURDATE() - INTERVAL 1 DAY
  ORDER BY sale_date, product_id;
```

There is no “Extra: Using filesort”:

```
+-------------+------+-------------+------+-------------+
| select_type | type | key         | rows | Extra       |
+-------------+------+-------------+------+-------------+
| SIMPLE      | ref  | sales_dt_pr |    1 | Using where |
+-------------+------+-------------+------+-------------+
```

The same execution plan is used when sorting for `PRODUCT_ID` only:

```
EXPLAIN
 SELECT sale_date, product_id, quantity
   FROM sales
  WHERE sale_date = CURDATE() - INTERVAL 1 DAY
  ORDER BY product_id;
```

Using greater or equals requires an explicit sort:

```
EXPLAIN
 SELECT sale_date, product_id, quantity
   FROM sales
  WHERE sale_date >= CURDATE() - INTERVAL 1 DAY
  ORDER BY product_id;
```

Execution plan re-formatted for a better fit on the page:

```
+-------------+------+-------------+------+----------------+
| select_type | type | key         | rows | Extra          |
+-------------+------+-------------+------+----------------+
| SIMPLE      | ref  | sales_dt_pr |  117 | Using where;   |
|             |      |             |      | Using filesort |
+-------------+------+-------------+------+----------------+
```

### Order By ASC/DESC and NULLS FIRST/LAST

MySQL uses the index backwards, but doesn’t mention it in the execution plan:

```
EXPLAIN
 SELECT sale_date, product_id, quantity
   FROM sales
  WHERE sale_date >= CURDATE() - INTERVAL 1 DAY
  ORDER BY sale_date DESC, product_id DESC;
```

Mixing `ASC` and `DESC` requires explicit sorting:

```
EXPLAIN
 SELECT sale_date, product_id, quantity
   FROM sales
  WHERE sale_date >= CURDATE() - INTERVAL 1 DAY
  ORDER BY sale_date ASC, product_id DESC;
```

MySQL Accepts `ASC` and `DESC` specification in the index definition, but ignores it.

```
ALTER TABLE sales
 DROP INDEX sales_dt_pr;

ALTER TABLE sales
  ADD INDEX sales_dt_pr (sale_date ASC, product_id DESC);

EXPLAIN
 SELECT sale_date, product_id, quantity
   FROM sales
  WHERE sale_date >= CURDATE() - INTERVAL 1 DAY
  ORDER BY sale_date ASC, product_id DESC;
```

To proof that, it is still avoiding the sort when both columns are sorted ascending.

```
EXPLAIN
 SELECT sale_date, product_id, quantity
   FROM sales
  WHERE sale_date >= CURDATE() - INTERVAL 1 DAY
  ORDER BY sale_date ASC, product_id ASC;
```

### Indexing Group By

An indexed `group by` show now sort operation in the execution plan:

```
EXPLAIN
 SELECT product_id, sum(eur_value)
   FROM sales
  WHERE sale_date = CURDATE() - INTERVAL 1 DAY
  GROUP BY product_id;
```

A regular `group by` is executed with the sort/group algorithm, because MySQL does not implement Hash-Group as of release 5.6.

```
EXPLAIN
 SELECT product_id, sum(eur_value)
   FROM sales
  WHERE sale_date >= CURDATE() - INTERVAL 1 DAY
  GROUP BY product_id;
```


## MySQL Example Scripts for “Partial Results”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/mysql/partial-results</sub>

This section contains the `create` and `insert` statements to run the examples from [Chapter 7*Partial Results*](partial-results.md) in a MySQL database.

### Querying Top-N Rows

An indexed Top-N query doesn’t show a “filesort” operation in the Extras column:

```
SELECT *
  FROM sales
 ORDER BY sale_date DESC
 LIMIT 10
```

```
+----+-------+-------+-------------+--------+-------+
| id | table | type  | key         | rows   | Extra |
+----+-------+-------+-------------+--------+-------+
|  1 | sales | index | sales_dt_pr | 836092 |       |
+----+-------+-------+-------------+--------+-------+
```


## MySQL Example Scripts for “3-Minute Quiz”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/mysql/3-minute-quiz</sub>

This section contains the `create`, `insert` and `select` statements for the “[Test your SQL Know-How in 3 Minutes](https://use-the-index-luke.com/3-minute-quiz)” test. You may want to test yourself before reading this page.

The `create` and `insert` statements are available in the [example schema archive](https://use-the-index-luke.com/use-the-index-luke.tar.gz).

The execution plans shown in this section are stripped to the relevant columns.

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

The first execution plan performs a full index scan (`type=index`). The second execution plan, on the other hand, performs an index range scan (`type=range`). Note the the rows column also reflects the more efficient access method.

```
+-------+---------------+---------+------+--------------------------+
| type  | possible_keys | key     | rows | Extra                    |
+-------+---------------+---------+------+--------------------------+
| index | NULL          | tbl_idx |  300 | Using where; Using index |
+-------+---------------+---------+------+--------------------------+
```

```
+-------+---------------+---------+------+--------------------------+
| type  | possible_keys | key     | rows | Extra                    |
+-------+---------------+---------+------+--------------------------+
| range | tbl_idx       | tbl_idx |  271 | Using where; Using index |
+-------+---------------+---------+------+--------------------------+
```

> **Tip:**
>
> - [Using Functions in the `WHERE` clause](where-clause-functions.md)
> - [Common Anti-Patterns: `DATE`](where-clause-obfuscation.md#date-types)
> - [Reading MySQL explain plan output](explain-plan-mysql.md#operations)

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

The query uses access type `REF`, which is very similar to the `RANGE` access. However, the interesting part is that there is no `SORT` mentioned in the Extra column.

```
+------+---------------+---------+-------------+
| type | possible_keys | key     | Extra       |
+------+---------------+---------+-------------+
| ref  | tbl_idx       | tbl_idx | Using where |
+------+---------------+---------+-------------+
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
DROP INDEX tbl_idx ON tbl;
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

The first query can use both indexes efficiently (`type=ref` or `range`).

```
+------+---------------+---------+------+-------+
| type | possible_keys | key     | rows | Extra |
+------+---------------+---------+------+-------+
| ref  | tbl_idx       | tbl_idx |    1 | NULL  |
+------+---------------+---------+------+-------+
```

```
+------+---------------+---------+------+-------+
| type | possible_keys | key     | rows | Extra |
+------+---------------+---------+------+-------+
| ref  | tbl_idx       | tbl_idx |    1 | NULL  |
+------+---------------+---------+------+-------+
```

The second query cannot use the key to access the index—it reads the entire index (`type=ALL`). Changing the column order in the index allows both queries to use the `REF` (or `RANGE`) access method.

```
+------+---------------+------+------+-------------+
| type | possible_keys | key  | rows | Extra       |
+------+---------------+------+------+-------------+
| ALL  | NULL          | NULL |  300 | Using where |
+------+---------------+------+------+-------------+
```

```
+------+---------------+---------+------+-------+
| type | possible_keys | key     | rows | Extra |
+------+---------------+---------+------+-------+
| ref  | tbl_idx       | tbl_idx |    2 | NULL  |
+------+---------------+---------+------+-------+
```

Note that MySQL might also do a full index scan (`type=Index`). Although this might be better than a full table scan (depends on the data), it is far less efficient than a index range scan (`type=ref` or `range`).

> **Tip:**
>
> - [The column order in Multi-Column Indexes](where-clause-the-equals-operator.md#concatenated-indexes)
> - [Reading MySQL explain plan output](explain-plan-mysql.md#operations)

### Question 4 — LIKE

```
CREATE INDEX tbl_idx ON tbl (text);
```

```
SELECT *
  FROM tbl
 WHERE text LIKE 'TJ%';
```

The execution plan clearly states that it is doing an [index range scan (type=range)](explain-plan-mysql.md#operations). Since there is only wild card character at the very end, the full search text `'TJ'` can be used as [index access predicate](where-clause-searching-for-ranges.md#greater-less-and-between).

```
+-------+---------+---------+------+-----------------------+
| type  | key     | key_len | ref  | Extra                 |
+-------+---------+---------+------+-----------------------+
| range | tbl_idx | 258     | NULL | Using index condition |
+-------+---------+---------+------+-----------------------+
```

> **Learn More:**
>
> - [A visual explanation why LIKE is slow](where-clause-searching-for-ranges.md#indexing-like-filters)

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

Although the index is used efficiently in both queries (access type `REF`), the second query does not mention “Using index” in the extra column. That means the second query is not executed as index-only scan and will perform less efficient as the first.

```
+------+---------------+---------+--------------------------+
| type | possible_keys | key     | Extra                    |
+------+---------------+---------+--------------------------+
| ref  | tbl_idx       | tbl_idx | Using where; Using index |
+------+---------------+---------+--------------------------+
```

```
+------+---------------+---------+------------------------------------+
| type | possible_keys | key     | Extra                              |
+------+---------------+---------+------------------------------------+
| ref  | tbl_idx       | tbl_idx | Using index condition; Using where |
+------+---------------+---------+------------------------------------+
```

> **Learn More:**
>
> - [Chapter 5*Clustering Data: The Second Power of Indexing*](clustering.md)
> - [Reading MySQL explain plan output](explain-plan-mysql.md#operations)
