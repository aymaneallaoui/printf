<!-- Source: https://use-the-index-luke.com/sql/example-schema/sql-server — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# SQL Server Example Scripts

<sub>Source: https://use-the-index-luke.com/sql/example-schema/sql-server</sub>

The scripts provided in this appendix are ready to run and were tested on the SQL Server database release 2008R2. Most of the examples will also work on earlier releases.

## Contents

1. *[The `where` clause](#sql-server-scripts-for-the-where-clause)*
2. *[Testing and Scalability](#sql-server-scripts-for-testing-and-scalability)*
3. *[The Join Operation](#sql-server-scripts-for-the-join-operation)*
4. *[Clustering Data](#sql-server-scripts-for-clustering-data)*
5. *[Sorting and Grouping](#sql-server-scripts-for-sorting-and-grouping)*
6. *[Partial Results](#sql-server-scripts-for-partial-results)*
7. *[Insert, Delete and Update](#sql-server-scripts-for-insert-delete-and-update)*
8. *[3-Minute Test](#sql-server-scripts-for-3-minute-quiz)*


## SQL Server Scripts for “The Where Clause”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/sql-server/where-clause</sub>

### The Equals Operator

#### Surrogate Keys

The following script creates the `EMPLOYEES` table with 1000 entries.

To generate random data, a view/function pair is used to bypass the “all user-defined functions are deterministic” feature of SQL Server.

```
CREATE TABLE employees (
    employee_id   NUMERIC       NOT NULL,
    first_name    VARCHAR(1000) NOT NULL,
    last_name     VARCHAR(900)  NOT NULL,
    date_of_birth DATE                   ,
    phone_number  VARCHAR(1000) NOT NULL,
    junk          CHAR(1000)             ,
    CONSTRAINT employees_pk
       PRIMARY KEY NONCLUSTERED (employee_id)
);
GO
```

```
IF OBJECT_ID('rand_helper') IS NOT NULL
   DROP VIEW rand_helper;
GO

CREATE VIEW rand_helper AS SELECT RND=RAND();
GO
```

```
IF OBJECT_ID('random_string') IS NOT NULL
   DROP FUNCTION random_string;
GO

CREATE FUNCTION random_string (@maxlen int)
   RETURNS VARCHAR(255)
AS BEGIN
   DECLARE @rv VARCHAR(255)
   DECLARE @loop int
   DECLARE @len int

   SET @len = (SELECT CAST(rnd * (@maxlen-3) AS INT) + 3
                 FROM rand_helper)
   SET @rv = ''
   SET @loop = 0

   WHILE @loop < @len BEGIN
      SET @rv = @rv
              + CHAR(CAST((SELECT rnd * 26
                             FROM rand_helper) AS INT )+97)
      IF @loop = 0 BEGIN
          SET @rv = UPPER(@rv)
      END
      SET @loop = @loop +1;
   END

   RETURN @rv
END
GO
```

```
IF OBJECT_ID('random_date') IS NOT NULL
   DROP FUNCTION random_date;
GO

CREATE FUNCTION random_date (@mindays int, @maxdays int)
   RETURNS VARCHAR(255)
AS BEGIN
   DECLARE @rv date
   SET @rv = (SELECT GetDate()
                   - rnd * (@maxdays-@mindays)
                   - @mindays
                FROM rand_helper)
   RETURN @rv
END
GO
```

```
IF OBJECT_ID('random_int') IS NOT NULL
   DROP FUNCTION random_int;
GO

CREATE FUNCTION random_int (@min int, @max int)
   RETURNS INT
AS BEGIN
   DECLARE @rv INT
   SET @rv = (SELECT rnd * (@max) + @min
                FROM rand_helper)
   RETURN @rv
END
GO
```

```
WITH generator (n) AS
( SELECT 1
   UNION ALL
  SELECT n + 1 FROM generator
WHERE n < 1000
)
INSERT INTO employees (employee_id
                     , first_name, last_name
                     , date_of_birth, phone_number, junk)
select n employee_id
     , [dbo].random_string(11) first_name
     , [dbo].random_string(11) last_name
     , [dbo].random_date(20*365, 60*365) dob
     , 'N/A' phone
     , 'junk' junk
  from generator
OPTION (MAXRECURSION 1000)
GO
```

```
UPDATE employees
   SET first_name='Markus',
       last_name='Winand'
 WHERE employee_id=123;

exec sp_updatestats;
GO
```

Notes:

- The `JUNK` column is used to have a realistic row length. Because it’s data type is `CHAR`, as opposed to `VARCHAR2`, it always needs the 1000 bytes it can hold. Without this column the table would become unrealistically small and many demonstrations would not work.
- Random data is filled into the table, with exception to my entry, that is updated after the insert.
- Table and index [statistics](where-clause-the-equals-operator.md#slow-indexes-part-ii) are gathered so that the [optimizer](where-clause-the-equals-operator.md#slow-indexes-part-ii) knows a little bit about the table’s content.
- SQL Server 2008R2 has a [900 byte index-key constraint](https://learn.microsoft.com/en-us/previous-versions/sql/sql-server-2008-r2/ms191241(v=sql.105)). Hence `LAST_NAME` reduced to 900.

#### Concatenated Keys

This script changes the `EMPLOYEES` table so that it reflects the situation after the merger with Very Big Company:

```
ALTER TABLE employees ADD subsidiary_id NUMERIC;
GO
UPDATE      employees SET subsidiary_id = 30;
GO
ALTER TABLE employees ALTER COLUMN subsidiary_id
                                   NUMERIC NOT NULL;
GO

ALTER TABLE employees DROP CONSTRAINT employees_pk;
GO
ALTER TABLE employees ADD  CONSTRAINT employees_pk
      PRIMARY KEY NONCLUSTERED (employee_id, subsidiary_id);
GO

WITH generator (n) as
( select 1
union all
select n + 1 from generator
where N < 9000
)
INSERT INTO employees (employee_id
                     , first_name, last_name
                     , date_of_birth, phone_number
                     , junk, subsidiary_id)
SELECT n employee_id
     , [dbo].random_string(11) first_name
     , [dbo].random_string(11) last_name
     , [dbo].random_date(20*365, 60*365) dob
     , 'N/A' phone
     , 'junk' junk
     , [dbo].random_int(1, (n*29)/9000) subsidiary_id
  FROM generator
OPTION (MAXRECURSION 9000)
GO

CREATE UNIQUE NONCLUSTERED INDEX
       employees_pk_tmp
       on employees (employee_id, subsidiary_id);
GO
ALTER TABLE employees DROP CONSTRAINT employees_pk;
GO
ALTER TABLE employees ADD CONSTRAINT employees_pk
      PRIMARY KEY NONCLUSTERED (employee_id, subsidiary_id);
GO
DROP INDEX employees_pk_tmp ON employees;
GO

exec sp_updatestats;
GO
```

Notes:

- The new primary key just extended by the `SUBSIDIARY_ID`; that is, the `EMPLOYEE_ID` remains in the first position.
- The new records are randomly assigned to the subsidiaries 1 through 29.
- The table and index are analyzed again to make the optimizer aware of the grown data volume.

The next script introduces the index on `SUBSIDIARY_ID` to support the query for all employees of one particular subsidiary:

```
CREATE NONCLUSTERED INDEX
       emp_sub_id ON employees (subsidiary_id);

exec sp_updatestats;
```

Notes:

- The table and all indexes are analyzed again.

Although that gives decent performance, it’s better to use the index that supports the primary key:

```
ALTER TABLE employees DROP CONSTRAINT employees_pk;
GO
ALTER TABLE employees ADD  CONSTRAINT employees_pk
      PRIMARY KEY NONCLUSTERED (subsidiary_id, employee_id);
GO

DROP INDEX emp_sub_id ON employees;

exec sp_updatestats;
```

Notes:

- The procedure leaves the table without primary key for a while.

  That means that the procedure is not suitable to run online. However, there is nothing accessing our test schema, therefore no danger.
- The index on `SUBSIDIARY_ID` is now fully redundant and can be dropped.

### Functions

SQL Server uses a case-insensitive collation per default. In that case, you don’t need a function-based index, a regular one will do as well:

```
CREATE INDEX emp_name ON employees (last_name);
```

### Filtered (Partial) Indexes

```
CREATE TABLE messages (
     id numeric not null,
     processed char(1) not null,
     receiver numeric not null,
     message varchar(255),
     primary key (id)
);

WITH generator (n) AS
( SELECT 1
   UNION ALL
  SELECT n + 1 FROM generator
WHERE n < 1000
)
INSERT INTO messages (id, processed, receiver, message)
select n id
     , case WHEN n % 5 =0 then 'N' else 'Y' end
     , n/10 receiver
     , 'junk' message
  from generator
OPTION (MAXRECURSION 1000)

-- regular index
--CREATE INDEX messages_todo
--          ON messages (receiver, processed) INCLUDE (message);

-- filtered index
CREATE INDEX messages_only_todo
          ON messages (receiver) INCLUDE (message)
       WHERE processed = 'N';

declare @r numeric
set @r = 4

SELECT message
   FROM messages
  WHERE processed = 'N'
    AND receiver  = @r;
```

At [SQL Fiddle](https://sqlfiddle.com/#!6/3a717/2).


## SQL Server Scripts for “Testing and Scalability”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/sql-server/performance-testing-scalability</sub>

This section contains the `create`, `insert` and T-SQL code to run the scalability test from [Chapter 3*Performance and Scalability*](testing-scalability.md) in an SQL Server database.

> **Warning:**
>
> These scripts will create large objects in the database and produce a huge amount of transaction logs.

It’s required to run the test against a very large data set to make sure caching does not affect the measurement. Depending on your environment, you might need to create even larger tables to reproduce a linear result as shown in the book.

```
CREATE TABLE scale_data (
   section NUMERIC NOT NULL,
   id1     NUMERIC NOT NULL,
   id2     NUMERIC NOT NULL,
   UNIQUE  (section, id1)
);
```

Note:

- There is no primary key (to keep the data generation simple).
- There is no index (yet). That’s done after filling the table.
- There is no “junk” column to keep the table small.

```
DECLARE @section INT
SET @section = 300

WHILE (@section >= 0) BEGIN

   WITH generate_series (n) AS (
      SELECT 1
      UNION ALL
      SELECT n + 1
        FROM generate_series
       WHERE N < 3000
   ), generate_series2 (n) AS (
      SELECT ROW_NUMBER() OVER(ORDER BY g1.n, g2.n)
        FROM generate_series g1
       CROSS JOIN generate_series g2
       WHERE g2.n <= @section
   )
   INSERT INTO scale_data
   SELECT @section, gen.*
        , CEILING(ABS(CAST(NEWID() AS BINARY(6)) %100))
     FROM generate_series2 gen
    WHERE gen.n <= @section * 3000
   OPTION(MAXRECURSION 32767);

   SET @section = @section -1
END;
GO
```

Note:

- This code generates 300 sections (highlighted). You may need to adjust the number for your environment.
- The table will need some gigabytes.

```
CREATE INDEX scale_slow ON scale_data(section, id1, id2);
GO
```

Note:

- The index will also need some gigabytes.
- That might take ages.

```
CREATE VIEW rand_helper AS SELECT rnd=RAND();
GO

CREATE FUNCTION [dbo].test_scalability (@n int)
   RETURNS @table TABLE
( section  NUMERIC NOT NULL PRIMARY KEY,
  duration NUMERIC NOT NULL,
  rows     NUMERIC NOT NULL)
AS BEGIN
   DECLARE @strt DATETIME2
   DECLARE @iter INT
   DECLARE @xsec INT
   DECLARE @xcnt INT
   DECLARE @xrnd INT

   SET @iter = 0
   WHILE (@iter < @n) BEGIN
      SET @xsec = 0
      WHILE (@xsec < 300) BEGIN
         SELECT @xrnd=CEILING(rnd * 100) FROM rand_helper;
         SET @strt = SYSDATETIME()

         SELECT @xcnt = COUNT(*)
           FROM (SELECT *
                   FROM scale_data
                  WHERE section=@xsec
                    AND id2=@xrnd) tlb;

         IF @iter = 0 BEGIN
           INSERT INTO @table
           VALUES ( @xsec
                  , datediff(microsecond, @strt, SYSDATETIME())
                  , @xcnt);
         END; ELSE BEGIN
           UPDATE @table
              SET duration = duration
                  + datediff(microsecond, @strt, SYSDATETIME())
                , rows = rows + @xcnt
            WHERE section = @xsec
         END;
         SET @xsec = @xsec + 1
      END;
      SET @iter = @iter + 1
   END;

   RETURN;
END;

GO
```

Note:

- The `SCALABILITY_SCALABILITY` function returns a table.
- It’s hard-coded to run the test 300 sections (highlighted).
- The number of iterations is configurable
- The `RAND_HELPER` view is required to bypass the use of `RAND()` in a function.

```
SELECT * FROM [dbo].[test_scalability] (10);
```

The counter test, with a better index, can be done like that:

```
CREATE INDEX scale_fast ON scale_data(section, id2, id1);
GO

SELECT * FROM [dbo].[test_scalability] (10);
GO
```


## SQL Server Scripts for “The Join Operation”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/sql-server/join</sub>

This section contains the `create`, `insert` and T-SQL code to run the examples from [Chapter 4*The Join Operation*](join.md) in a SQL Server database. It requires the helper functions `RANDOM_DATE` and `RANDOM_INT` from the [where clause examples](#sql-server-scripts-for-the-where-clause).

```
CREATE TABLE sales (
  sale_id       NUMERIC NOT NULL,
  employee_id   NUMERIC NOT NULL,
  subsidiary_id NUMERIC NOT NULL,
  sale_date     DATE    NOT NULL,
  eur_value     NUMERIC(17,2) NOT NULL,
  product_id    NUMERIC NOT NULL,
  quantity      NUMERIC NOT NULL,
  junk          CHAR(200),
  CONSTRAINT sales_pk
     PRIMARY KEY NONCLUSTERED (sale_id),
  CONSTRAINT sales_emp_fk
     FOREIGN KEY          (subsidiary_id, employee_id)
      REFERENCES employees(subsidiary_id, employee_id)
);
GO

SELECT RAND(0);
GO

WITH generator (n)
  AS (
     SELECT 1
      UNION ALL
     SELECT n + 1
       FROM generator
      WHERE N < 1800
     )
INSERT INTO sales (sale_id
                 , subsidiary_id, employee_id
                 , sale_date, eur_value
                 , product_id, quantity
                 , junk)
SELECT row_number() OVER (ORDER BY sale_date), data.*
  FROM (
       SELECT e.subsidiary_id, e.employee_id
            , [dbo].random_date(0, 3650) sale_date
            , [dbo].random_int(1, 100000)/100 eur_value
            , [dbo].random_int(1, 25) product_id
            , [dbo].random_int(1, 5) quantity
            , 'junk' junk
         FROM employees e
            , generator gen
        WHERE employee_id % 7 = 4
          AND gen.n < employee_id / 5
       ) data
        WHERE DATEPART(weekday, sale_date)
           <> DATEPART(weekday, '2012-01-01')
        ORDER BY sale_date
OPTION(MAXRECURSION 2000);
GO

EXEC sp_updatestats;
GO
```

Notes:

- The rows are inserted chronologically to reflect a natural table growth.
- Only a small fraction of employees have sales at all.


## SQL Server Scripts for “Clustering Data”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/sql-server/clustering-data</sub>

This section contains the `create`, `insert` and T-SQL code to run the examples from [Chapter 5*Clustering Data: The Second Power of Indexing*](clustering.md) in a SQL Server database. It requires the helper functions `RANDOM_DATE` and `RANDOM_INT` from the [where clause examples](#sql-server-scripts-for-the-where-clause).

### Index-Organized Table (Clustered Index)

The following creates a second sales table with a clustered index and a secondary index on `SALE_DATE`.

```
CREATE TABLE sales_clst (
  sale_id       NUMERIC NOT NULL,
  employee_id   NUMERIC NOT NULL,
  subsidiary_id NUMERIC NOT NULL,
  sale_date     DATE    NOT NULL,
  eur_value     NUMERIC(17,2) NOT NULL,
  junk          CHAR(200),
  CONSTRAINT sales_pk
     PRIMARY KEY (sale_id),
  CONSTRAINT sales_emp_fk
     FOREIGN KEY          (subsidiary_id, employee_id)
      REFERENCES employees(subsidiary_id, employee_id)
);
GO

SELECT RAND(0);
GO

WITH generator (n)
  AS (
     SELECT 1
      UNION ALL
     SELECT n + 1
       FROM generator
      WHERE N < 1800
     )
INSERT INTO sales_clst (sale_id
                      , subsidiary_id, employee_id
                      , sale_date, eur_value, junk)
SELECT row_number() OVER (ORDER BY sale_date), data.*
  FROM (
       SELECT e.subsidiary_id, e.employee_id
            , [dbo].random_date(0, 3650) sale_date
            , [dbo].random_int(1, 100000)/100 eur_value
            , 'junk' junk
         FROM employees e
            , generator gen
        WHERE employee_id % 7 = 4
          AND gen.n < employee_id / 5
       ) data
        ORDER BY sale_date
OPTION(MAXRECURSION 2000);
GO

EXEC sp_updatestats;
GO
```


## SQL Server Scripts for “Sorting and Grouping”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/sql-server/sorting-grouping</sub>

This section contains the code and execution plans for [Chapter 6*Sorting and Grouping*](sorting-grouping.md) in a SQL Server database.

### Indexed Order By

```
DROP INDEX sales_date ON sales;
GO

CREATE INDEX sales_dt_pr ON sales (sale_date, product_id);
GO

EXEC sp_updatestats;
GO

SET STATISTICS PROFILE ON;

SELECT sale_date, product_id, quantity
  FROM sales
 WHERE sale_date = DATEADD(day, -1, GETDATE())
 ORDER BY  sale_date, product_id;
```

The execution does not perform a sort operation:

```
Nested Loops(Inner Join, OUTER REFERENCES:[Bmk1000])
 |--Index Seek(OBJECT:([sales].[sales_dt_pr]),
 |  SEEK:[sales].[sale_date]=dateadd(day,(-1),getdate())
 |  ORDERED FORWARD)
 |--RID Lookup(OBJECT:([sales]),
    SEEK:[Bmk1000]=[Bmk1000]) LOOKUP ORDERED FORWARD
```

SQL Server uses the same execution plan, when sorting by `PRODUCT_ID` only.

```
SELECT sale_date, product_id, quantity
  FROM sales
 WHERE sale_date = DATEADD(day, -1, GETDATE())
 ORDER BY product_id;
```

Using an greater or equals condition requires an Sort operation:

```
SELECT sale_date, product_id, quantity
  FROM sales
 WHERE sale_date >= DATEADD(day, -1, GETDATE())
 ORDER BY product_id;
```

Although the row estimate got lower, causing the cost also to be lower:

```
Sort(ORDER BY:([test].[dbo].[sales].[product_id] ASC))
 |--Nested Loops(Inner Join, OPTIMIZED WITH UNORDERED PREFETCH)
    |--Compute Scalar(DEFINE:([Expr1009]=BmkToPage([Bmk1000])))
    |  |--Nested Loops(Inner Join)
    |     |--Compute Scalar([...])
    |     |  |--Constant Scan
    |     |--Index Seek(OBJECT:([sales].[sales_dt_pr]),
    |        SEEK:([sales].[sale_date] > [Expr1007]
    |         AND  [sales].[sale_date] < NULL) ORDERED FORWARD)
    |--RID Lookup(OBJECT:([sales]),
       SEEK:([Bmk1000]=[Bmk1000]) LOOKUP ORDERED FORWARD)
```

### Indexing ASC, DESC and NULLS FIRST/LAST

Scanning an index backwards:

```
SELECT sale_date, product_id, quantity
  FROM sales
 WHERE sale_date >= DATEADD(day, -1, GETDATE())
 ORDER BY sale_date DESC, product_id DESC;
```

Mixing `ASC` and `DESC` causes an explicit sort:

```
SELECT sale_date, product_id, quantity
  FROM sales
 WHERE sale_date >= DATEADD(day, -1, GETDATE())
 ORDER BY sale_date ASC, product_id DESC;
```

Ordering the index with mixed `ASC`/`DESC` modifiers:

```
DROP INDEX sales_dt_pr ON sales;
GO

CREATE INDEX sales_dt_pr
    ON sales (sale_date ASC, product_id DESC);
GO

SELECT sale_date, product_id, quantity
  FROM sales
 WHERE sale_date >= DATEADD(day, -1, GETDATE())
 ORDER BY sale_date ASC, product_id DESC;
```

SQL Server 2008R2 does not implement the `order by` `NULLS` extension.

```
SELECT sale_date, product_id, quantity
  FROM sales
 WHERE sale_date >= DATEADD(day, -1, GETDATE())
 ORDER BY sale_date ASC, product_id DESC NULLS LAST;
```

### Indexed Group By

Pipelined `group by` execution:

```
SELECT product_id, SUM(eur_value)
  FROM sales
 WHERE sale_date = DATEADD(day, -1, GETDATE())
 GROUP BY product_id;
```

Explicit Sort/Group when retrieving the stats for two days (parallelism disable for plan readability):

```
SELECT product_id, SUM(eur_value)
  FROM sales
 WHERE sale_date >= DATEADD(day, -1, GETDATE())
 GROUP BY product_id
OPTION (MAXDOP 1);
```

The Hash-Algorithm is used when aggregating a larger set:

```
SELECT product_id, SUM(eur_value)
  FROM sales
 WHERE sale_date >= DATEADD(day, -100, GETDATE())
 GROUP BY product_id
OPTION (MAXDOP 1);
```


## SQL Server Scripts for “Partial Results”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/sql-server/partial-results</sub>

This section contains the `create` and `insert` statements to run the examples from [Chapter 7*Partial Results*](partial-results.md) in an SQL Server database.

### Querying Top-N Rows

The test approach for the scalability of Top-N queries is the same as used in the “[Testing and Scalability](#sql-server-scripts-for-testing-and-scalability)” chapter.

```
CREATE FUNCTION [dbo].test_top_n_scalability (@n int)
   RETURNS @table TABLE
( section  NUMERIC NOT NULL PRIMARY KEY,
  duration NUMERIC NOT NULL,
  rows     NUMERIC NOT NULL)
AS BEGIN
   DECLARE @strt DATETIME2
   DECLARE @iter INT
   DECLARE @xsec INT
   DECLARE @xcnt INT
   DECLARE @xrnd INT

   SET @iter = 0
   WHILE (@iter < @n) BEGIN
      SET @xsec = 0
      WHILE (@xsec < 300) BEGIN
         SET @strt = SYSDATETIME()

         SELECT @xcnt = COUNT(*)
           FROM (SELECT TOP 100 *
                   FROM scale_data
                  WHERE section=@xsec
                  ORDER BY id2) tlb;

         IF @iter = 0 BEGIN
           INSERT INTO @table
           VALUES ( @xsec
                  , datediff(microsecond, @strt, SYSDATETIME())
                  , @xcnt);
         END; ELSE BEGIN
           UPDATE @table
              SET duration = duration
                  + datediff(microsecond, @strt, SYSDATETIME())
                , rows = rows + @xcnt
            WHERE section = @xsec
         END;
         SET @xsec = @xsec + 1
      END;
      SET @iter = @iter + 1
   END;

   RETURN;
END;

GO
```

First, using a pipelined Top-N with an index covering the `order by` clause:

```
CREATE INDEX scale_fast ON scale_data(section, id2, id1);
GO

SELECT * FROM [dbo].[test_top_n_scalability] (10);
GO
```

Then, using an index for the `where` clause only. However, SQL Server refuses to use the index unless it includes the ID2 column. So, it is not exactly the same test case as for the other databases, but it still shows the scalability.

```
DROP INDEX scale_fast ON scale_data;
GO

CREATE INDEX scale_slow ON scale_data(section, id1, id2);
GO

SELECT * FROM [dbo].[test_top_n_scalability] (10);
GO
```

### Paging Through Results

```
CREATE FUNCTION [dbo].test_topn_scalability (@n int)
   RETURNS @table TABLE
( section NUMERIC NOT NULL,
  mode    NUMERIC NOT NULL,
  page    NUMERIC NOT NULL,
  seconds NUMERIC NOT NULL)
AS BEGIN
  DECLARE @strt DATETIME2
  DECLARE @iter INT
  DECLARE @xmde INT
  DECLARE @page INT
  DECLARE @xsec INT
  DECLARE @c1 INT, @c2 INT;
  DECLARE @cont TABLE (
    section int NOT NULL,
    c1      int NOT NULL,
    c2      int NOT NULL
  );

  SET @iter = 0
  WHILE (@iter < @n) BEGIN
    SET @xmde = 0
    WHILE (@xmde <= 1) BEGIN
      SET @page = 0
      WHILE (@page <= 100) BEGIN
        SET @xsec = 5
        WHILE (@xsec < 300) BEGIN
          SET @strt = SYSDATETIME()

          IF @xmde = 0 OR @page = 0 BEGIN
            DECLARE sql CURSOR FAST_FORWARD FOR
             SELECT id2, id1
               FROM (SELECT id2, id1
                          , ROW_NUMBER() OVER (ORDER BY id2, id1) rn
                       FROM scale_data
                      WHERE section = @xsec
                    ) result
              WHERE rn >  100 * (@page  )
                AND rn <= 100 * (@page+1);

          END; ELSE BEGIN
            SELECT @c2 = c2, @c1 = c1 FROM @cont WHERE section = @xsec;
            DECLARE sql CURSOR FAST_FORWARD FOR
             SELECT TOP 100 id2, id1
               FROM scale_data
              WHERE section = @xsec
                AND id2 >= @c2
                AND (
                       (id2 = @c2 AND id1 > @c1)
                    OR
                       (id2 > @c2)
                    )
              ORDER BY id2, id1
          END;

          OPEN sql; FETCH NEXT FROM sql INTO @c2, @c1;
          WHILE @@FETCH_STATUS = 0 BEGIN
            FETCH NEXT FROM sql INTO @c2, @c1;
          END
          CLOSE sql; DEALLOCATE sql;

          INSERT INTO @table
          VALUES ( @xsec
                 , @xmde
                 , @page
                 , datediff(microsecond, @strt, SYSDATETIME())
                 );
          UPDATE @cont set c1 = @c1, c2 = @c2
           WHERE section = @xsec;
          IF @@ROWCOUNT = 0
           INSERT INTO @cont VALUES(@xsec, @c1, @c2);
          SET @xsec = @xsec + 1
        END;
        SET @page = @page + 1
      END;
      SET @xmde = @xmde + 1
    END;
    SET @iter = @iter + 1
  END;

  RETURN;
END;
GO

SELECT section, mode, page, sum(seconds)
  FROM [dbo].[test_topn_scalability] (10)
 WHERE section=10
 GROUP BY section, mode, page
 ORDER BY section, mode, page;
GO
```

### Window-Functions

SQL Server 2008R2 utilizes the index to implement a pipelined Top-N query when using the window-Function `ROW_NUMBER`:

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
|-Sort(ORDER BY:([sale_date] DESC, [sale_id] DESC))
  |-Filter(WHERE:([Expr1004]>=(11) AND [Expr1004]<=(20)))
    |-Top(TOP EXPRESSION:(20))
      |-Sequence Project(DEFINE:([Expr1004]=row_number))
        |-Segment
          |-Nested Loops(Inner Join, WITH ORDERED PREFETCH)
            |-Index Scan([sales].[sl_dtid], ORDERED BACKWARD)
            |-RID Lookup([sales],
               SEEK:([Bmk1000]=[Bmk1000])
               LOOKUP ORDERED FORWARD)
```

SQL Server reads the index backwards, so that it doesn’t need an sort operation for the window function. The Top-Step aborts the operations below it, as soon as 20 rows arrived. The last two steps, shown first in the execution plan, filter the leading 10 rows off and sorts the remaining result. This sort operation will just sort ten rows, so it is not going to be a performance problem.


## SQL Server Scripts for “Insert, Delete and Update”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/sql-server/dml</sub>

This section contains the `create` and `insert` statements to run the examples from [Chapter 8*Modifying Data*](dml.md) in an SQL Server database. There is only one query that reports all figures for the `insert`, `delete` and `update` sections.

```
WITH generate_series_1k(n) AS (
   SELECT 0
    UNION ALL
   SELECT n + 1
     FROM generate_series_1k
    WHERE N + 1 < 10000
), generate_series(n, n1, n2) AS (
   SELECT gs2.n * 1000 + gs1.n, gs1.n, gs2.n
     FROM generate_series_1k gs1,
          generate_series_1k gs2
    WHERE gs1.n < 1000
)
SELECT n id1
     , CAST(CEILING(ABS(CAST(NEWID() AS BINARY(6)) % 900000))
       + 100000 AS numeric) id2
     , CAST(CEILING(ABS(CAST(NEWID() AS BINARY(6)) % 900000))
       + 100000 AS numeric) id3
     , CAST(CEILING(ABS(CAST(NEWID() AS BINARY(6)) % 900000))
       + 100000 AS numeric) id4
     , CAST(CEILING(ABS(CAST(NEWID() AS BINARY(6)) % 900000))
       + 100000 AS numeric) id5
  INTO scale_write_0
  FROM generate_series
OPTION(MAXRECURSION 32767);
GO

SELECT *
  INTO scale_write_1
  FROM scale_write_0;
GO

SELECT *
  INTO scale_write_2
  FROM scale_write_0;
GO

SELECT *
  INTO scale_write_3
  FROM scale_write_0;
GO

SELECT *
  INTO scale_write_4
  FROM scale_write_0;
GO

SELECT *
  INTO scale_write_5
  FROM scale_write_0;
GO

CREATE INDEX scale_write_1_1 on scale_write_1(id1);
GO

CREATE INDEX scale_write_2_1 on scale_write_2(id1);
GO
CREATE INDEX scale_write_2_2 on scale_write_2(id2, id1);
GO

CREATE INDEX scale_write_3_1 on scale_write_3(id1);
GO
CREATE INDEX scale_write_3_2 on scale_write_3(id2, id1);
GO
CREATE INDEX scale_write_3_3 on scale_write_3(id3, id2, id1);
GO

CREATE INDEX scale_write_4_1 on scale_write_4(id1);
GO
CREATE INDEX scale_write_4_2 on scale_write_4(id2, id1);
GO
CREATE INDEX scale_write_4_3 on scale_write_4(id3, id2, id1);
GO
CREATE INDEX scale_write_4_4 on scale_write_4(id4, id3, id2
                                             ,id1);
GO

CREATE INDEX scale_write_5_1 on scale_write_5(id1);
GO
CREATE INDEX scale_write_5_2 on scale_write_5(id2, id1);
GO
CREATE INDEX scale_write_5_3 on scale_write_5(id3, id2, id1);
GO
CREATE INDEX scale_write_5_4 on scale_write_5(id4, id3, id2
                                             ,id1);
GO
CREATE INDEX scale_write_5_5 on scale_write_5(id5, id4, id3
                                             ,id2, id1);
GO
```

```
CREATE PROCEDURE
run_insert(@idxes INT, @q INT, @n INT, @mode VARCHAR(64) OUT)
AS BEGIN
DECLARE @r2  INT = CEILING(rand() * 9000000)+1000000;
DECLARE @r3  INT = CEILING(rand() * 9000000)+1000000;
DECLARE @r4  INT = CEILING(rand() * 9000000)+1000000;
DECLARE @r5  INT = CEILING(rand() * 9000000)+1000000;
DECLARE @d1  INT;
  WHILE (@n > 0) BEGIN
  SET @d1 = CEILING(rand() * @q);
  IF @idxes = 0
         INSERT INTO scale_write_0 (id1, id2, id3, id4, id5)
                            VALUES (@d1, @r2, @r3, @r4, @r5);
  ELSE IF @idxes = 1
         INSERT INTO scale_write_1 (id1, id2, id3, id4, id5)
                            VALUES (@d1, @r2, @r3, @r4, @r5);
  ELSE IF @idxes = 2
         INSERT INTO scale_write_2 (id1, id2, id3, id4, id5)
                            VALUES (@d1, @r2, @r3, @r4, @r5);
  ELSE IF @idxes = 3
         INSERT INTO scale_write_3 (id1, id2, id3, id4, id5)
                            VALUES (@d1, @r2, @r3, @r4, @r5);
  ELSE IF @idxes = 4
         INSERT INTO scale_write_4 (id1, id2, id3, id4, id5)
                            VALUES (@d1, @r2, @r3, @r4, @r5);
  ELSE IF @idxes = 5
         INSERT INTO scale_write_5 (id1, id2, id3, id4, id5)
                            VALUES (@d1, @r2, @r3, @r4, @r5);
  SET @n = @n - 1;
  END;
  SET @mode = 'insert';
END;
go

CREATE PROCEDURE
run_delete(@idxes INT, @q INT, @n INT, @mode VARCHAR(64) OUT)
AS BEGIN
DECLARE @cnt INT = @n;
DECLARE @aff INT = 0;
DECLARE @d1  INT;
  WHILE (@cnt >0) BEGIN
  SET @d1 = CEILING(rand() * @q) + @q;
  IF @idxes = 1
         DELETE FROM scale_write_1 WHERE id1 = @d1;
  ELSE IF @idxes = 2
         DELETE FROM scale_write_2 WHERE id1 = @d1;
  ELSE IF @idxes = 3
         DELETE FROM scale_write_3 WHERE id1 = @d1;
  ELSE IF @idxes = 4
         DELETE FROM scale_write_4 WHERE id1 = @d1;
  ELSE IF @idxes = 5
         DELETE FROM scale_write_5 WHERE id1 = @d1;
  SET @aff = @aff + @@ROWCOUNT;
  SET @cnt = @cnt - 1;
  END;
  SET @mode = CASE WHEN @aff = @n THEN 'delete' ELSE NULL END;
END;

go

CREATE PROCEDURE
run_update_all(@idxes INT, @q INT, @n INT, @mode VARCHAR(64) OUT)
AS BEGIN
DECLARE @r2  INT = CEILING(rand() * 9000000)+1000000;
DECLARE @r3  INT = CEILING(rand() * 9000000)+1000000;
DECLARE @r4  INT = CEILING(rand() * 9000000)+1000000;
DECLARE @r5  INT = CEILING(rand() * 9000000)+1000000;
DECLARE @cnt INT = @n;
DECLARE @aff INT = 0;
DECLARE @d1  INT;
  WHILE (@cnt >0) BEGIN
    SET @d1 = CEILING(rand() * @q) + 2 * @q;
    IF @idxes = 1
       UPDATE scale_write_1
          SET id2=@r2, id3=@r3, id4=@r4, id5=@r5 WHERE id1=@d1;
    ELSE IF @idxes = 2
       UPDATE scale_write_2
          SET id2=@r2, id3=@r3, id4=@r4, id5=@r5 WHERE id1=@d1;
    ELSE IF @idxes = 3
       UPDATE scale_write_3
          SET id2=@r2, id3=@r3, id4=@r4, id5=@r5 WHERE id1=@d1;
    ELSE IF @idxes = 4
       UPDATE scale_write_4
          SET id2=@r2, id3=@r3, id4=@r4, id5=@r5 WHERE id1=@d1;
    ELSE IF @idxes = 5
       UPDATE scale_write_5
          SET id2=@r2, id3=@r3, id4=@r4, id5=@r5 WHERE id1=@d1;

    SET @aff = @aff + @@ROWCOUNT;
    SET @cnt = @cnt - 1;
  END;
  SET @mode = CASE WHEN @aff = @n THEN 'update all'
                   ELSE NULL END;
END;
go

CREATE PROCEDURE
run_update_one(@idxes INT, @q INT, @n INT, @mode VARCHAR(64) OUT)
AS BEGIN
DECLARE @r2 INT = CEILING(rand() * 9000000)+1000000;
DECLARE @cnt INT = @n;
DECLARE @aff INT = 0;
DECLARE @d1  INT;
  WHILE (@cnt >0) BEGIN
    SET @d1 = CEILING(rand() * @q) + 3 * @q;
    IF @idxes = 1 -- no index updated
           UPDATE scale_write_1 SET id2 = @r2 WHERE id1=@d1;
    ELSE IF @idxes = 2 -- one index updated
           UPDATE scale_write_2 SET id2 = @r2 WHERE id1=@d1;
    ELSE IF @idxes = 3 -- one index updated
           UPDATE scale_write_3 SET id3 = @r2 WHERE id1=@d1;
    ELSE IF @idxes = 4 -- one index updated
         UPDATE scale_write_4 SET id4 = @r2 WHERE id1=@d1;
    ELSE IF @idxes = 5 -- one index updated
         UPDATE scale_write_5 SET id5 = @r2 WHERE id1=@d1;
    ELSE SET @aff = 0;
    SET @mode = CASE WHEN @@ROWCOUNT = 1 THEN 'update one'
                     ELSE NULL END;
    SET @aff = @aff + @@ROWCOUNT;
    SET @cnt = @cnt - 1;
  END;
  SET @mode = CASE WHEN @aff = @n THEN 'update one'
                   ELSE NULL END;
END;
go

CREATE PROCEDURE
test_write_scalability (@n int, @inner int)
AS BEGIN
DECLARE @iter INT;
DECLARE @indxs  INT;
DECLARE @strt DATETIME2;
DECLARE @dur  NUMERIC;
DECLARE @cmnd INT;
DECLARE @q    INT;
DECLARE @c1   VARCHAR(64);
DECLARE @table TABLE
( indxes  NUMERIC NOT NULL,
  mode    VARCHAR(64) NOT NULL,
  seconds NUMERIC NOT NULL,
  cnt     NUMERIC NOT NULL);

  SELECT @q = (max(id1) - min(id1))/4 FROM scale_write_1;
  SET @iter = 0;
  WHILE (@iter < @n) BEGIN
    SET @cmnd = 0;
    WHILE (@cmnd <= 3) BEGIN
      SET @indxs = 0;
      WHILE (@indxs <= 5) BEGIN
        SET @strt = SYSDATETIME();
        IF (@cmnd = 0)
           exec [dbo].run_insert     @indxs, @q, @inner
                                   , @mode=@c1 OUTPUT;
        ELSE IF (@cmnd = 1)
           exec [dbo].run_update_all @indxs, @q, @inner
                                   , @mode=@c1 OUTPUT;
        ELSE IF (@cmnd = 2)
           exec [dbo].run_update_one @indxs, @q, @inner
                                   , @mode=@c1 OUTPUT;
        ELSE IF (@cmnd = 3) BEGIN
           exec [dbo].run_delete     @indxs, @q, @inner
                                   , @mode=@c1 OUTPUT;
        END;

        SET @dur = datediff(microsecond, @strt, SYSDATETIME());
        IF @c1 IS NOT NULL
        BEGIN
          INSERT INTO @table
          VALUES (@indxs, @c1, @dur, 1);
        END;
        SET @indxs = @indxs +1;
      END;
      SET @cmnd = @cmnd +1;
    END;
    SET @iter = @iter + 1;
  END;
  SELECT indxes, mode, seconds, cnt from @table;
END;
```

```
SET NOCOUNT ON;

CREATE TABLE #res (
  indxes  NUMERIC NOT NULL,
  mode    VARCHAR(64) NOT NULL,
  seconds NUMERIC NOT NULL,
  cnt     NUMERIC NOT NULL);
GO

INSERT INTO #res
EXEC test_write_scalability 1000;
go

SELECT indxes, [insert], [delete], [update all], [update one]
  FROM (SELECT indxes, mode, seconds/1000000 seconds
          FROM #res
       ) x
         PIVOT (AVG(seconds)
           FOR mode
            IN ([insert],[delete],[update all],[update one]))
            AS AvgExecTime
 ORDER BY indxes;
```


## SQL Server Scripts for “3-Minute Quiz”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/sql-server/3-minute-quiz</sub>

This section contains the `create`, `insert` and `select` statements for the “[Test your SQL Know-How in 3 Minutes](https://use-the-index-luke.com/3-minute-quiz)” test. You may want to test yourself before reading this page.

The `create` and `insert` statements are available in the [example schema archive](https://use-the-index-luke.com/use-the-index-luke.tar.gz).

### Question 1 — DATE Anti-Pattern

```
CREATE INDEX tbl_idx ON tbl (date_column);
```

```
SELECT COUNT(*)
  FROM tbl
 WHERE DATEPART(YEAR, date_column) = 2025;
```

```
SELECT COUNT(*)
  FROM tbl
 WHERE date_column >= {d'2025-01-01'}
   AND date_column <  {d'2026-01-01'};
```

```
|--Compute Scalar(DEFINE:([Expr1003]=CONVERT_IMPLICIT(int,[Expr1005],0)))
   |--Stream Aggregate(DEFINE:([Expr1005]=Count(*)))
      |--Index Scan(OBJECT:(tbl.tbl_idx),
             WHERE:(datepart(year,tbl.date_column)=(2017)))
```

```
|--Compute Scalar(DEFINE:([Expr1003]=CONVERT_IMPLICIT(int,[Expr1013],0)))
   |--Stream Aggregate(DEFINE:([Expr1013]=Count(*)))
      |--Nested Loops(Inner Join)
         |--Merge Interval
         |  |--Concatenation
         |     |--Compute Scalar(DEFINE:(([Expr1005],[Expr1006],[Expr1004])=...))
         |     |  |--Constant Scan
         |     |--Compute Scalar(DEFINE:(([Expr1008],[Expr1009],[Expr1007])=...))
         |        |--Constant Scan
         |--Index Seek(OBJECT:(tbl.tbl_idx),
                 SEEK:(tbl.date_column > [Expr1010]
                   AND tbl.date_column < [Expr1011])
               ORDERED FORWARD)
```

> **Learn More:**
>
> - [Using Functions in the `WHERE` clause](where-clause-functions.md)
> - [Common Anti-Patterns: `DATE`](where-clause-obfuscation.md#date-types)
> - [Explain plan operations `Index Scan` and `Index Seek`](explain-plan-sql-server.md#operations)

### Question 2 — Indexed Top-N

```
CREATE INDEX tbl_idx ON tbl (a, date_column);
```

```
SELECT TOP 1*
  FROM tbl
 WHERE a = 12
 ORDER BY date_column DESC
 ;
```

The `Index Seek` returns the result `BACKWARD` to reflect the `ORDER BY DESC`. Note that there is no `SORT` operation.

```
|--Top(TOP EXPRESSION:(1))
   |--Nested Loops(Inner Join)
      |--Index Seek(OBJECT:(tbl.tbl_idx),
      |       SEEK:(a=12.)
      |      ORDERED BACKWARD)
      |--Clustered Index Seek(OBJECT:(tbl.PK__tbl__3213E83F20C1E124),
                        SEEK:(tbl.id=tbl.id)
                       LOOKUP ORDERED FORWARD)
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

The first query can use both index very efficiently (`Index Seek` with only `SEEK` predicates):

```
|--Nested Loops(Inner Join)
   |--Index Seek(OBJECT:(tbl.tbl_idx),
   |       SEEK:(tbl.a=38. AND tbl.b=1.)
   |     ORDERED FORWARD)
   |--Clustered Index Seek(OBJECT:(tbl.PK__tbl__3213E83F20C1E124),
                     SEEK:(tbl.id=tbl.id)
                    LOOKUP ORDERED FORWARD)
```

```
|--Nested Loops(Inner Join)
   |--Index Seek(OBJECT:(tbl.tbl_idx),
   |       SEEK:(tbl.b=1. AND tbl.a=38.)
   |     ORDERED FORWARD)
   |--Clustered Index Seek(OBJECT:(tbl.PK__tbl__3213E83F20C1E124),
                     SEEK:(tbl.id=tbl.id)
                    LOOKUP ORDERED FORWARD)
```

The second query cannot perform an `Index Seek`, so it makes an `Index Scan`—reading the entire index.

```
|--Nested Loops(Inner Join)
   |--Index Scan(OBJECT:(tbl.tbl_idx),
   |      WHERE:(tbl.b=1.))
   |--Clustered Index Seek(OBJECT:(tbl.PK__tbl__3213E83F20C1E124),
                     SEEK:(tbl.id=tbl.id)
                     LOOKUP ORDERED FORWARD)
```

With the reversed index column order, the second query can use the index efficiently:

```
|--Nested Loops(Inner Join)
   |--Index Seek(OBJECT:(tbl.tbl_idx),
   |       SEEK:(tbl.b=1.)
   |      ORDERED FORWARD)
   |--Clustered Index Seek(OBJECT:(tbl.PK__tbl__3213E83F20C1E124],
                     SEEK:(tbl.id=tbl.id)
                    LOOKUP ORDERED FORWARD)
```

> **Learn More:**
>
> - [The column order in Multi-Column Indexes](where-clause-the-equals-operator.md#concatenated-indexes)
> - [Explain plan operations `Index Scan` and `Index Seek`](explain-plan-sql-server.md#operations)

### Question 4 — LIKE

```
CREATE INDEX tbl_idx ON tbl (text);
```

```
SELECT *
  FROM tbl
 WHERE text LIKE 'TJ%';
```

The execution plan states that it is doing an *Index Seek*. Since there is only wild card character at the very end, the full search text `'TERM'` can be used as [index access predicate](where-clause-searching-for-ranges.md#greater-less-and-between).

```
|--Nested Loops(Inner Join, OUTER REFERENCES:(tbl.id))
   |--Index Seek(OBJECT:(tbl.tbl_idx)
   |             , SEEK:(tbl.text >= 'TÏþ'
   |                 AND tbl.text <  'TK')
   |             ,  WHERE:(tbl.text like 'TJ%')
   |             ORDERED FORWARD
   |            )
   |--Clustered Index Seek(OBJECT:(tbl.tbl_pk)
                 , SEEK:(tbl.id=tbl.id)
                 LOOKUP ORDERED FORWARD
                )
```

> **Learn More:**
>
> - [A visual explanation why LIKE is slow](where-clause-searching-for-ranges.md#indexing-like-filters)
> - [Explain plan operations `Index Scan` and `Index Seek`](explain-plan-sql-server.md#operations)

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

The first query uses one index to search based on column `A` but can also retrieve the `DATE_COLUMN` from the very same index. The second query must run a key lookup on the clustered index (or `RID Lookup (HEAP)`) to apply the filter on the `B` column.

```
|--Compute Scalar(DEFINE:([Expr1003]=CONVERT_IMPLICIT(int,[Expr1006],0)))
   |--Stream Aggregate(GROUP BY:(tbl.date_column)
   |  DEFINE:([Expr1006]=Count(*)))
   |--Index Seek(OBJECT:(tbl.tbl_idx),
           SEEK:(tbl.a=38.)
          ORDERED FORWARD)
```

```
|--Compute Scalar(DEFINE:([Expr1003]=CONVERT_IMPLICIT(int,[Expr1006],0)))
   |--Stream Aggregate(GROUP BY:(tbl.date_column)
      |  DEFINE:([Expr1006]=Count(*)))
      |--Nested Loops(Inner Join)
         |--Index Seek(OBJECT:(tbl.tbl_idx),
         |       SEEK:(tbl.a=38.)
         |     ORDERED FORWARD)
         |--Clustered Index Seek(OBJECT:(tbl.PK__tbl__3213E83F20C1E124),
                           SEEK:(tbl.id=tbl.id),
                          WHERE:(tbl.b=1.)
                          LOOKUP ORDERED FORWARD)
```

> **Learn More:**
>
> - [Chapter 5*Clustering Data: The Second Power of Indexing*](clustering.md)
> - [Reading SQL Server explain plan output](explain-plan-sql-server.md#operations)
