<!-- Source: https://use-the-index-luke.com/sql/example-schema/oracle — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# Oracle Example Scripts

<sub>Source: https://use-the-index-luke.com/sql/example-schema/oracle</sub>

The scripts provided in this appendix are ready to run and were tested on the Oracle database release 11gR2. Most of the examples will also work on 10g.

## Contents

1. *[The `where` clause](#oracle-example-scripts-for-the-where-clause)*
2. *[Testing and Scalability](#oracle-example-scripts-for-testing-and-scalability)*
3. *[The Join Operation](#oracle-example-scripts-for-the-join-operation)*
4. *[Clustering Data](#oracle-example-scripts-for-clustering-data)*
5. *[Sorting and Grouping](#oracle-example-scripts-for-sorting-and-grouping)*
6. *[Partial Results](#oracle-example-scripts-for-partial-results)*
7. *[Insert, Delete and Update](#oracle-example-scripts-for-insert-delete-and-update)*
8. *[3-Minute Test](#oracle-example-scripts-for-3-minute-quiz)*


## Oracle Example Scripts for “The Where Clause”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/oracle/where-clause</sub>

### The Equals Operator

#### Surrogate Keys

The following script creates the `EMPLOYEES` table with 1000 entries.

```
CREATE TABLE employees (
   employee_id   NUMBER         NOT NULL,
   first_name    VARCHAR2(1000) NOT NULL,
   last_name     VARCHAR2(1000) NOT NULL,
   date_of_birth DATE           NOT NULL,
   phone_number  VARCHAR2(1000) NOT NULL,
   junk          CHAR(1000)     DEFAULT 'JUNK',
   CONSTRAINT employees_pk PRIMARY KEY (employee_id)
);
```

```
INSERT INTO employees (employee_id,  first_name,
                       last_name,    date_of_birth,
                       phone_number)
SELECT level,
       DBMS_RANDOM.STRING('u', 1) ||
            DBMS_RANDOM.STRING('l', DBMS_RANDOM.value(2,10)),
       DBMS_RANDOM.STRING('u', 1) ||
            DBMS_RANDOM.STRING('l', DBMS_RANDOM.value(2,10)),
       SYSDATE - (DBMS_RANDOM.normal() * 365 * 10) - 40 * 365,
       TRUNC(DBMS_RANDOM.VALUE(1000,10000))
  FROM DUAL
  CONNECT BY level <= 1000;
```

```
UPDATE employees
   SET first_name='MARKUS',
       last_name='WINAND'
 WHERE employee_id=123;
```

```
BEGIN
     DBMS_STATS.GATHER_TABLE_STATS(null, 'EMPLOYEES',
     METHOD_OPT=>'for all indexed columns', CASCADE => true);
END;
```

Notes:

- The `JUNK` column is used to have a realistic row length. Because it’s data type is `CHAR`, as opposed to `VARCHAR2`, it always needs the 1000 bytes it can hold. Without this column the table would become unrealistically small and many demonstrations would not work.
- Random data is filled into the table, with exception to my entry, that is updated after the insert.
- Table and index [statistics](where-clause-the-equals-operator.md#slow-indexes-part-ii) are gathered so that the [optimizer](where-clause-the-equals-operator.md#slow-indexes-part-ii) knows a little bit about the table’s content.

#### Concatenated Keys

This script changes the `EMPLOYEES` table so that it reflects the situation after the merger with Very Big Company:

```
-- add subsidiary_id and update existing records
ALTER TABLE employees ADD subsidiary_id NUMBER;
```

```
UPDATE      employees SET subsidiary_id = 30;
```

```
ALTER TABLE employees MODIFY subsidiary_id NOT NULL;
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
                       phone_number, subsidiary_id)
SELECT level,
       DBMS_RANDOM.STRING('u', 1) ||
            DBMS_RANDOM.STRING('l', DBMS_RANDOM.value(2,10)),
       DBMS_RANDOM.STRING('u', 1) ||
            DBMS_RANDOM.STRING('l', DBMS_RANDOM.value(2,10)),
       SYSDATE - (DBMS_RANDOM.normal() * 365 * 10) - 40 * 365,
       TRUNC(DBMS_RANDOM.VALUE(1000,10000)),
       TRUNC(DBMS_RANDOM.VALUE(1,level/9000*29))
FROM DUAL CONNECT BY level <= 9000;
```

```
BEGIN
     DBMS_STATS.GATHER_TABLE_STATS(null, 'EMPLOYEES',
     METHOD_OPT=>'for all indexed columns', CASCADE => true);
END;
```

Notes:

- The new primary key just extended by the `SUBSIDIARY_ID`; that is, the `EMPLOYEE_ID` remains in the first position.
- The new records are randomly assigned to the subsidiaries 1 through 29.
- The table and index are analyzed again to make the optimizer aware of the grown data volume.

The next script introduces the index on `SUBSIDIARY_ID` to support the query for all employees of one particular subsidiary:

```
CREATE INDEX emp_sub_id ON employees (subsidiary_id);
```

```
BEGIN
     DBMS_STATS.GATHER_TABLE_STATS(null, 'EMPLOYEES',
     METHOD_OPT=>'for all indexed columns', CASCADE => true);
END;
```

Notes:

- The table and all indexes are analyzed again. In that particular case it would be sufficient to analyse only the new index.

Although that gives decent performance, it’s better to use the index that supports the primary key:

Oracle 11g
:   ```
    -- use tmp index to support the PK
    CREATE INDEX employee_pk_tmp
        ON employees (subsidiary_id, employee_id, 1);
    ```

    ```
    ALTER TABLE employees
          MODIFY CONSTRAINT employees_pk
          USING INDEX employee_pk_tmp;
    ```

    ```
    -- recreate the pk index as needed (automatically done)
    --DROP   INDEX employee_pk;
    ```

    ```
    CREATE UNIQUE INDEX employee_pk
        ON employees (subsidiary_id, employee_id);
    ```

    ```
    -- change the constraint to use the new index
    ALTER TABLE employees
          MODIFY CONSTRAINT employees_pk
          USING INDEX employee_pk;
    ```

    ```
    -- drop old indexes
    DROP INDEX employee_pk_tmp;
    ```

    ```
    DROP INDEX emp_sub_id;
    ```

    ```
    BEGIN
         DBMS_STATS.GATHER_TABLE_STATS(null, 'EMPLOYEES',
         METHOD_OPT=>'for all indexed columns', CASCADE => true);
    END;
    ```

Oracle 12c
:   ```
    CREATE UNIQUE INDEX employee_pk_new
        ON employees (subsidiary_id, employee_id);
    ```

    ```
    ALTER TABLE employees
          MODIFY CONSTRAINT employees_pk
          USING INDEX employee_pk_new;
    ```

    ```
    -- drop old indexes
    DROP INDEX emp_sub_id;
    ```

    ```
    -- note: employee_pk is automatically dropped

    -- rename new PK index
    ALTER INDEX employee_pk_new RENAME TO employee_pk;
    ```

    ```
    BEGIN
         DBMS_STATS.GATHER_TABLE_STATS(null, 'EMPLOYEES',
         METHOD_OPT=>'for all indexed columns', CASCADE => true);
    END;
    ```

Notes:

- In versions prior 12c, we create a new index with a dummy column and use it to temporarily support the PK.

  This is required because the Oracle database till 11g doesn’t allow two indexes that include the same columns.
- Once the old PK index isn’t used by the constraint anymore, it can be dropped and recreated with its new column order.
- The constraint is changed again to use the new PK index and the temporary index can be dropped—as well as the index on the subsidiary id that isn’t required anymore.

#### Slow Indexes, Part II

The following statement removes some statistics to make my example work.

```
BEGIN
      DBMS_STATS.DELETE_COLUMN_STATS
       (null, 'EMPLOYEES', 'SUBSIDIARY_ID');
END
```

To re-create them, use the same procedure as before:

```
BEGIN
     DBMS_STATS.GATHER_TABLE_STATS(null, 'EMPLOYEES',
     METHOD_OPT=>'for all indexed columns', CASCADE => true);
END
```

The final statement to create, and analyze, the index on the `LAST_NAME` column:

```
CREATE INDEX emp_name ON employees (last_name)
```

```
BEGIN
     DBMS_STATS.GATHER_TABLE_STATS(null, 'EMPLOYEES',
     METHOD_OPT=>'for all indexed columns', CASCADE => true);
END
```

### Functions

#### Case-Insensitive Search

The randomized names were already created in correct case, just update “my” record:

```
UPDATE employees
   SET first_name = 'Markus'
     , last_name  = 'Winand'
 WHERE employee_id   = 123
   AND subsidiary_id = 30;;
```

The statement to create the function-based index:

```
CREATE INDEX emp_up_name ON employees (UPPER(last_name));;
DROP INDEX emp_name;;
```

Notes:

- I intentionally break my own best practice to re-analyze the table and all indexes. Just the new index is analyzed (automatically as of 10g).

The next statement will, as of release 11g, automatically collect the extended statistics for the function-based index.

```
BEGIN
     DBMS_STATS.GATHER_TABLE_STATS(null, 'EMPLOYEES',
     METHOD_OPT=>'for all indexed columns', CASCADE => true);
END;
/
```

#### User-Defined Functions

Define a PL/SQL function that calculates the age and attempts to use it in an index:

```
CREATE FUNCTION get_age(date_of_birth DATE)
RETURN NUMBER
AS
BEGIN
    RETURN TRUNC(MONTHS_BETWEEN(SYSDATE, DATE_OF_BIRTH)/12);
END;
/

CREATE INDEX invalid ON EMPLOYEES (get_age(date_of_birth));
```

You should get the error “ORA-30553: The function is not deterministic”.

### Searching for Ranges

Creating the `EMP_TEST` index:

```
CREATE INDEX emp_test
     ON employees (date_of_birth, subsidiary_id);;
```

And the reverse column order:

```
CREATE INDEX emp_test
     ON employees (subsidiary_id, date_of_birth);;
```

### Indexing NULL

The following re-creates the standard indexes after you have run the examples from the book:

```
-- for demo purpose we drop the NOT NULL constraint
ALTER TABLE employees MODIFY date_of_birth NULL;;
CREATE INDEX emp_dob ON employees (date_of_birth);;
```

```
DROP INDEX emp_dob_upname;;
```

```
CREATE INDEX emp_dob ON employees (date_of_birth, '1');;
```

### Emulating Partial Indexes

```
CREATE TABLE messages AS
SELECT level id
     , CASE WHEN DBMS_RANDOM.NORMAL() < 0.09 THEN 'N' ELSE 'Y' END processed
     , trunc(DBMS_RANDOM.VALUE(0,100)) receiver
     , RPAD('junk', 200) message
  FROM dual
CONNECT BY level < 999999;;
```

You have to update the statistics after creating the function-based index. No statistics, no CBO, no FBI.

```
begin
   DBMS_STATS.GATHER_TABLE_STATS( user
                                ,'MESSAGES'
                                , cascade=>true);
end;
/
```


## Oracle Example Scripts for “Testing and Scalability”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/oracle/performance-testing-scalability</sub>

This section contains the `create`, `insert` and PL/SQL code to run the scalability test from [Chapter 3*Performance and Scalability*](testing-scalability.md) in an Oracle 11gR2 database.

> **Warning:**
>
> These scripts will create large objects in the database and produce a huge amount of redo logs.

```
CREATE TABLE scale_data (
   section NUMBER NOT NULL,
   id1     NUMBER NOT NULL,
   id2     NUMBER NOT NULL
);
```

Note:

- There is no primary key (to keep the data generation simple)
- There is no index (yet). That’s done after filling the table
- There is no “junk” column because the table is actually not accessed during testing

```
INSERT INTO scale_data
SELECT sections.n, gen.x, CEIL(DBMS_RANDOM.VALUE(0, 100))
  FROM (
         SELECT level - 1 n
           FROM DUAL
        CONNECT BY level < 300) sections
       , (
         SELECT level x
           FROM DUAL
        CONNECT BY level < 900000) gen
 WHERE gen.x <= sections.n * 3000;
```

Note:

- This code generates 300 sections, you may need to adjust the number for your environment. If you increase the number of sections, you must also increase the second generator. It must generate at least `3000 x <number of sections>` records.
- The table will need some gigabytes

```
CREATE INDEX scale_slow ON scale_data (section, id1, id2);

BEGIN
     DBMS_STATS.GATHER_TABLE_STATS(null, 'SCALE_DATA'
                                       , CASCADE => true);
END;
/
```

Note:

- The index will also need some gigabytes

```
CREATE OR REPLACE PACKAGE test_scalability IS
  TYPE piped_output IS RECORD ( section  NUMBER
                              , seconds  NUMBER
                              , cnt_rows NUMBER);
  TYPE piped_output_table IS TABLE OF piped_output;

  FUNCTION run(sql_txt IN varchar2, n IN number)
    RETURN test_scalability.piped_output_table PIPELINED;
END;
/

CREATE OR REPLACE PACKAGE BODY test_scalability
IS
  TYPE tmp IS TABLE OF piped_output INDEX BY PLS_INTEGER;

  FUNCTION run(sql_txt IN VARCHAR2, n IN NUMBER)
    RETURN test_scalability.piped_output_table PIPELINED
  IS
    rec  test_scalability.tmp;
    r    test_scalability.piped_output;
    iter NUMBER;
    sec  NUMBER;
    strt NUMBER;
    exec_txt VARCHAR2(4000);
    cnt  NUMBER;
  BEGIN
    exec_txt := 'select count(*) from (' || sql_txt || ')';
    iter := 0;
    WHILE iter <= n LOOP
      sec := 0;
      WHILE sec < 300 LOOP
        IF iter = 0 THEN
           rec(sec).seconds  := 0;
           rec(sec).section  := sec;
           rec(sec).cnt_rows := 0;
        END IF;
        strt := DBMS_UTILITY.GET_TIME;
        EXECUTE IMMEDIATE exec_txt INTO cnt USING sec;
        rec(sec).seconds := rec(sec).seconds
                          + (DBMS_UTILITY.GET_TIME - strt)/100;
        rec(sec).cnt_rows:= rec(sec).cnt_rows + cnt;
        IF iter = n THEN
          PIPE ROW(rec(sec));
        END IF;
        sec := sec +1;
      END LOOP;
      iter := iter +1;
    END LOOP;
    RETURN;
  END;
END test_scalability;
/
```

Note:

- The `TEST_SCALABILITY.RUN` function returns a table
- It’s hardcoded to run the test for 300 sections (highlighted).
- The number of iterations is configurable

The following `select` calls the function and passes the query as string:

```
SELECT *
  FROM TABLE(test_scalability.run(
       'SELECT * '
      || 'FROM scale_data '
      ||'WHERE section=:1 '
      ||  'AND id2=CEIL(DBMS_RANDOM.value(1,100))', 10));
```

The counter test, with a better index, can be done like that:

```
DROP INDEX scale_slow;
CREATE INDEX scale_fast ON scale_data (section, id2, id1);

BEGIN
     DBMS_STATS.GATHER_TABLE_STATS(null, 'SCALE_DATA'
                                       , CASCADE => true);
END;
/

SELECT *
  FROM TABLE(test_scalability.run(
       'SELECT * '
      || 'FROM scale_data '
      ||'WHERE section=:1 '
      ||  'AND id2=CEIL(DBMS_RANDOM.value(1,10))', 10));
```

Note:

- The `SCALE_SLOW` index is dropped to prevent “ORA-01408: such column list already indexed”.


## Oracle Example Scripts for “The Join Operation”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/oracle/join</sub>

This section contains the `create`, `insert` and PL/SQL code to run the examples from [Chapter 4*The Join Operation*](join.md) in an Oracle 11gR2 database.

```
CREATE TABLE sales (
  sale_id       NUMBER NOT NULL,
  employee_id   NUMBER NOT NULL,
  subsidiary_id NUMBER NOT NULL,
  sale_date     DATE   NOT NULL,
  eur_value     NUMBER(17,2) NOT NULL,
  product_id    NUMBER NOT NULL,
  quantity      number NOT NULL,
  junk          CHAR(200),
  CONSTRAINT sales_pk
     PRIMARY KEY (sale_id),
  CONSTRAINT sales_emp_fk
     FOREIGN KEY          (subsidiary_id, employee_id)
      REFERENCES employees(subsidiary_id, employee_id)
);

EXEC DBMS_RANDOM.SEED(0);

INSERT INTO sales (sale_id
                 , subsidiary_id, employee_id
                 , sale_date, eur_value
                 , product_id, quantity
                 , junk)
SELECT rownum, data.*
  FROM (
       SELECT e.subsidiary_id, e.employee_id
            , TRUNC(SYSDATE
                  - DBMS_RANDOM.VALUE(0, 3650)) sale_date
            , DBMS_RANDOM.VALUE(10,10000)/100 eur_value
            , TRUNC(DBMS_RANDOM.VALUE(1,25)) product_id
            , TRUNC(DBMS_RANDOM.VALUE(1,5)) quantity
            , 'junk'
         FROM employees e
            , ( SELECT level n
                  FROM dual
               CONNECT BY level < 1800
              ) gen
        WHERE MOD(employee_id, 7) = 4
          AND gen.n < employee_id / 5
        ORDER BY sale_date
       ) data
 WHERE TO_CHAR(sale_date, 'D')
    != TO_CHAR(TO_DATE('2012-01-01', 'YYYY-MM-DD'), 'D');

BEGIN
     DBMS_STATS.GATHER_TABLE_STATS(null, 'SALES',
     METHOD_OPT=>'for all indexed columns', CASCADE => true);
END;
/
```

Notes:

- The rows are inserted chronologically to reflect a natural table growth.
- Only a small fraction of employees have sales at all.
- No sales on Sundays. This is, however, hard to accomplish because [Oracle’s `TO_CHAR` is sensitive to `NLS_TERRITORY`](https://renenyffenegger.ch/notes/development/databases/Oracle/SQL/functions/type-conversion/to/char/index) settings. Using `TO_CHAR` on both sides cancels that effect—so, it is implemented by a comparison of the weekday for a known Sunday (1st Jan 2012).


## Oracle Example Scripts for “Clustering Data”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/oracle/clustering-data</sub>

This section contains the `create` and `insert` statements to run the examples from [Chapter 5*Clustering Data: The Second Power of Indexing*](clustering.md) in an Oracle 11gR2 database.

### Index-Organized Table

The following creates a second sales table as index-organized table. A secondary index on `SALE_DATE` is created.

```
CREATE TABLE sales_iot (
  sale_id       NUMBER NOT NULL,
  employee_id   NUMBER NOT NULL,
  subsidiary_id NUMBER NOT NULL,
  sale_date     DATE   NOT NULL,
  eur_value     NUMBER(17,2) NOT NULL,
  junk          CHAR(200),
  CONSTRAINT sales_iot_pk
     PRIMARY KEY (sale_id),
  CONSTRAINT sales_iot_emp_fk
     FOREIGN KEY          (subsidiary_id, employee_id)
      REFERENCES employees(subsidiary_id, employee_id)
) ORGANIZATION INDEX;

EXEC DBMS_RANDOM.SEED(0);

INSERT INTO sales_iot (sale_id
                     , subsidiary_id, employee_id
                     , sale_date, eur_value, junk)
SELECT rownum, data.*
  FROM (
       SELECT e.subsidiary_id, e.employee_id
            , TRUNC (SYSDATE
                   - DBMS_RANDOM.VALUE(0, 3650)) sale_date
            , DBMS_RANDOM.VALUE(10,10000)/100 eur_value
            , 'junk'
         FROM employees e
            , ( SELECT level n
                  FROM dual
               CONNECT BY level < 1800
              ) gen
        WHERE MOD(employee_id, 7) = 4
          AND gen.n < employee_id / 5
        ORDER BY sale_date
       ) data;

CREATE INDEX sales_iot_date ON sales_iot (sale_date);

BEGIN
     DBMS_STATS.GATHER_TABLE_STATS(null, 'SALES_IOT',
     METHOD_OPT=>'for all indexed columns', CASCADE => true);
END;
/
```


## Oracle Example Scripts for “Sorting and Grouping”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/oracle/sorting-grouping</sub>

This section contains the `create`, `insert` and PL/SQL code to run the examples from [Chapter 6*Sorting and Grouping*](sorting-grouping.md) in an Oracle 11gR2 database.

### Indexed Order By

```
SELECT sale_date, product_id, quantity
  FROM sales
 WHERE sale_date >= TRUNC(sysdate) - INTERVAL '1' DAY
 ORDER BY product_id
```

Gathering new statistics is good practice after changing indexes:

```
BEGIN
     DBMS_STATS.GATHER_TABLE_STATS(null, 'SALES',
     METHOD_OPT=>'for all indexed columns', CASCADE => true);
END;
/
```

### Indexed Group By

There is one particular problem in the Oracle database (at least 11g-19c) that appears when ordering the grouped result in reverse index order:

```
SELECT product_id, sum(eur_value)
  FROM sales
 WHERE sale_date = TRUNC(sysdate) - INTERVAL '1' DAY
 GROUP BY product_id
 ORDER BY product_id DESC;
```

Although it can use the index when ordering in index order:

```
SELECT product_id, sum(eur_value)
  FROM sales
 WHERE sale_date = TRUNC(sysdate) - INTERVAL '1' DAY
 GROUP BY product_id
 ORDER BY product_id ASC;
```

There is no known workaround for this problem.


## Oracle Example Scripts for “Partial Results”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/oracle/partial-results</sub>

This section contains the `create` and `insert` statements to run the examples from [Chapter 7*Partial Results*](partial-results.md) in an Oracle database.

The test approach for the scalability of Top-N queries is the same as used in the “[Testing and Scalability](#oracle-example-scripts-for-testing-and-scalability)” chapter.

### Querying Top-N Rows

First, using a pipelined Top-N with an index covering the `order by` clause:

```
CREATE INDEX scale_slow ON scale_data (section, id1, id2);

SELECT *
  FROM TABLE(test_scalability.run(
       'SELECT * FROM (SELECT id2, id1 '
                    ||  'FROM scale_data '
                    || 'WHERE section=:1 '
                    || 'ORDER BY id2, id1) '
    || ' WHERE rownum <= 100', 10));
```

Then, using an index for the `where` clause only:

```
  DROP INDEX scale_slow;
CREATE INDEX scale_fast ON scale_data (SECTION, id2, id1);

SELECT *
  FROM TABLE(test_scalability.run(
       'SELECT * FROM (SELECT id2, id1 '
                    ||  'FROM scale_data '
                    || 'WHERE section=:1 '
                    || 'ORDER BY id2, id1) '
    || ' WHERE rownum <= 100', 10));
```

### Paging Through Results

The following function uses both methods to fetch the result page-wise. The select statement in the end prepares the statistics on screen.

```
CREATE OR REPLACE
PACKAGE test_topn_scalability IS
  TYPE piped_output IS
             RECORD ( section  NUMBER
                    , mde      NUMBER
                    , page     NUMBER
                    , seconds  INTERVAL DAY TO SECOND);
  TYPE piped_output_table IS TABLE OF piped_output;

  FUNCTION run(n IN number)
    RETURN test_topn_scalability.piped_output_table PIPELINED;
END;
/

CREATE OR REPLACE
PACKAGE BODY test_topn_scalability
IS
  TYPE tmp IS TABLE OF piped_output INDEX BY PLS_INTEGER;

FUNCTION run(n IN NUMBER)
  RETURN test_topn_scalability.piped_output_table PIPELINED
IS
  TYPE last_fetched IS RECORD (id2 NUMBER, id1 NUMBER);
  last last_fetched;
  rec  test_topn_scalability.piped_output;
  TYPE sec_array IS TABLE OF last_fetched INDEX BY PLS_INTEGER;

  iter NUMBER;
  sec  NUMBER;
  strt TIMESTAMP(9);
  mde  NUMBER;
  page NUMBER;
  cont sec_array;

  CURSOR s_restart (sec IN NUMBER, page IN NUMBER)
      IS SELECT id2, id1
           FROM (SELECT id2, id1, rownum rn
                   FROM scale_data
                  WHERE section = sec
                  ORDER BY id2, id1)
          WHERE rownum <= 100
            AND rn > page*100;

  CURSOR s_continue (sec IN NUMBER, c IN last_fetched)
      IS SELECT *
           FROM (SELECT id2, id1
                   FROM scale_data
                  WHERE section = sec
                    AND id2 >= c.id2
                    AND (
                            (id2 = c.id2 AND id1 > c.id1)
                         OR
                            (id2 > c.id2)
                        )
                  ORDER BY id2, id1)
          WHERE rownum <= 100;
BEGIN
  iter := 0;
  WHILE iter <= n LOOP
    FOR mde IN 0 .. 1 LOOP
      FOR page IN 0 .. 100 LOOP
        FOR sec IN 0 .. 300 LOOP
          strt := systimestamp;
          IF (mde = 0 OR page = 0) THEN
            FOR r IN s_restart (sec, page) LOOP
              last := r;
            END LOOP;
          ELSE
            FOR r IN s_continue (sec, cont(sec)) LOOP
              last := r;
            END LOOP;
          END IF;

          rec.seconds := (systimestamp - strt);
          rec.section := sec;
          rec.page    := page;
          rec.mde     := mde;
          PIPE ROW(rec);

          cont(sec) := last;

        END LOOP;
      END LOOP;
    END LOOP;
    iter := iter +1;
  END LOOP;
  RETURN;
END run;
END test_topn_scalability;
/

SELECT section, mde, page, sum(extract(second from seconds))
  FROM TABLE(test_topn_scalability.run(10))
 WHERE section = 10
 GROUP BY section, mde, page
 ORDER BY section, mde, page;
```


## Oracle Example Scripts for “Insert, Delete and Update”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/oracle/dml</sub>

This section contains the `create` and `insert` statements to run the examples from [Chapter 8*Modifying Data*](dml.md) in an Oracle database. There is only one query that reports all figures for the `insert`, `delete` and `update` sections.

```
CREATE TABLE scale_write_0 AS
  WITH generator AS (
                     SELECT --+ materialize
                            level n
                       FROM DUAL
                    CONNECT BY level <= 10000
)
 SELECT rownum id1
      , CEIL(DBMS_RANDOM.VALUE(1000000,9999999)) id2
      , CEIL(DBMS_RANDOM.VALUE(1000000,9999999)) id3
      , CEIL(DBMS_RANDOM.VALUE(1000000,9999999)) id4
      , CEIL(DBMS_RANDOM.VALUE(1000000,9999999)) id5
   FROM generator, generator
  WHERE rownum <= 10000000;

CREATE TABLE scale_write_1 AS
SELECT * from scale_write_0;

CREATE TABLE scale_write_2 AS
SELECT * from scale_write_0;

CREATE TABLE scale_write_3 AS
SELECT * from scale_write_0;

CREATE TABLE scale_write_4 AS
SELECT * from scale_write_0;

CREATE TABLE scale_write_5 AS
SELECT * from scale_write_0;

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
                                            , id1);

CREATE INDEX scale_write_5_1 on scale_write_5(id1);
CREATE INDEX scale_write_5_2 on scale_write_5(id2, id1);
CREATE INDEX scale_write_5_3 on scale_write_5(id3, id2, id1);
CREATE INDEX scale_write_5_4 on scale_write_5(id4, id3, id2
                                           , id1);
CREATE INDEX scale_write_5_5 on scale_write_5(id5, id4, id3
                                           , id2, id1);

begin
 DBMS_STATS.GATHER_TABLE_STATS(user
                             , 'SCALE_WRITE_0', cascade=>true);
 DBMS_STATS.GATHER_TABLE_STATS(user
                             , 'SCALE_WRITE_1', cascade=>true);
 DBMS_STATS.GATHER_TABLE_STATS(user
                             , 'SCALE_WRITE_2', cascade=>true);
 DBMS_STATS.GATHER_TABLE_STATS(user
                             , 'SCALE_WRITE_3', cascade=>true);
 DBMS_STATS.GATHER_TABLE_STATS(user
                             , 'SCALE_WRITE_4', cascade=>true);
 DBMS_STATS.GATHER_TABLE_STATS(user
                             , 'SCALE_WRITE_5', cascade=>true);
end;
/
```

```
create or replace
PACKAGE test_write_scalability IS
  TYPE piped_output IS
             RECORD ( idxes   NUMBER
                    , cmnd    VARCHAR2(255)
                    , seconds NUMBER
                    , id1     NUMBER);
  TYPE piped_output_table IS TABLE OF piped_output;

  FUNCTION run(n IN number)
    RETURN test_write_scalability.piped_output_table PIPELINED;
END;

create or replace
PACKAGE BODY test_write_scalability
IS
  TYPE tmp IS TABLE OF piped_output INDEX BY PLS_INTEGER;

FUNCTION run_insert(tbl IN NUMBER, d1 IN NUMBER)
                    RETURN VARCHAR2
AS
  r2 NUMBER := CEIL(DBMS_RANDOM.VALUE(1000000,9999999));
  r3 NUMBER := CEIL(DBMS_RANDOM.VALUE(1000000,9999999));
  r4 NUMBER := CEIL(DBMS_RANDOM.VALUE(1000000,9999999));
  r5 NUMBER := CEIL(DBMS_RANDOM.VALUE(1000000,9999999));
BEGIN
  CASE tbl
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
  RETURN 'insert';
END;

FUNCTION run_delete(tbl IN NUMBER, d1 IN NUMBER)
RETURN VARCHAR2
AS
BEGIN
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
  IF SQL%ROWCOUNT > 0 THEN RETURN 'delete';
  ELSE RETURN NULL; END IF;
END;

FUNCTION run_update_all(tbl IN NUMBER, d1 IN NUMBER)
RETURN VARCHAR2
AS
  r2 NUMBER := CEIL(DBMS_RANDOM.VALUE(1000000,9999999));
  r3 NUMBER := CEIL(DBMS_RANDOM.VALUE(1000000,9999999));
  r4 NUMBER := CEIL(DBMS_RANDOM.VALUE(1000000,9999999));
  r5 NUMBER := CEIL(DBMS_RANDOM.VALUE(1000000,9999999));
BEGIN
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
  IF SQL%ROWCOUNT > 0 THEN RETURN 'update all';
  ELSE RETURN NULL; END IF;
END;

FUNCTION run_update_one(tbl IN NUMBER, d1 IN NUMBER)
RETURN VARCHAR2
AS
  r NUMBER := CEIL(DBMS_RANDOM.VALUE(1000000,9999999));
BEGIN
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
  IF SQL%ROWCOUNT > 0 THEN RETURN 'update one';
  ELSE RETURN NULL; END IF;
END;

FUNCTION run(n IN NUMBER)
  RETURN test_write_scalability.piped_output_table PIPELINED
IS
  PRAGMA AUTONOMOUS_TRANSACTION;
  rec  test_write_scalability.piped_output;

  id1  NUMBER;
  tbl  NUMBER;
  strt TIMESTAMP(9);
  cmnd NUMBER;
  d1   NUMBER;
  q    NUMBER;
  begn NUMBER;
  iter NUMBER;
  r    NUMBER;
  tmp  DATE;

BEGIN
  SELECT CEIL((max(id1)-min(id1))/4) into q FROM scale_write_1;

  iter := n;
  WHILE iter > 0 LOOP
    FOR cmd IN 0 .. 3 LOOP
      r := TRUNC(DBMS_RANDOM.VALUE(0, q));
      FOR tbl IN 0 .. 5 LOOP
        strt := systimestamp;
        rec.cmnd :=
        CASE cmd
        WHEN 0 THEN run_update_all(tbl, r + cmd*q)
        WHEN 1 THEN run_insert    (tbl, r + cmd*q)
        WHEN 2 THEN run_update_one(tbl, r + cmd*q)
        WHEN 3 THEN run_delete    (tbl, r + cmd*q)
        END;
        IF rec.cmnd IS NOT NULL THEN
          COMMIT;
          -- magic: convert INTERVAL DAYS TO SECONDS
          -- to NUMERIC (seconds)
          tmp := sysdate;
          rec.seconds := tmp
                       + (systimestamp - strt)*86400
                       - tmp;
          rec.idxes   := tbl;
          rec.id1     := r + cmd*q;
          PIPE ROW(rec);
        END IF;
      END LOOP;
    END LOOP;
    iter := iter - 1;
  END LOOP;
  COMMIT;
  RETURN;
END run;
END test_write_scalability;
```

```
SELECT *
  FROM (SELECT idxes, cmnd, seconds
          FROM TABLE (test_write_scalability.run(1000)
       )
 PIVOT (AVG(seconds)
   FOR cmnd
    IN ('insert', 'delete', 'update all', 'update one')
       );
```


## Oracle Example Scripts for “3-Minute Quiz”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/oracle/3-minute-quiz</sub>

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

The first execution plan performs a full table scan (`TABLE ACCESS FULL`). The second execution plan, on the other hand, performs an `INDEX RANGE SCAN`.

```
-----------------------------------------------------
| Id | Operation          | Name | A-Rows | Buffers |
-----------------------------------------------------
|  0 | SELECT STATEMENT   |      |      1 |       7 |
|  1 |  SORT AGGREGATE    |      |      1 |       7 |
|* 2 |   TABLE ACCESS FULL| TBL  |    271 |       7 |
-----------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------

2 - filter(EXTRACT(YEAR FROM DATE_COLUMN)=2017)
```

```
-------------------------------------------------------
| Id | Operation         | Name    | A-Rows | Buffers |
-------------------------------------------------------
|  0 | SELECT STATEMENT  |         |      1 |       1 |
|  1 |  SORT AGGREGATE   |         |      1 |       1 |
|* 2 |   INDEX RANGE SCAN| TBL_IDX |    271 |       1 |
-------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------

2 - access(DATE_COLUMN>=TO_DATE('2017-01-01 00:00:00', 'syyyy-mm-dd hh24:mi:ss')
       AND DATE_COLUMN< TO_DATE('2018-01-01 00:00:00', 'syyyy-mm-dd hh24:mi:ss'))
```

> **Learn More:**
>
> - [Using Functions in the `WHERE` clause](where-clause-functions.md)
> - [Common Anti-Patterns: `DATE`](where-clause-obfuscation.md#date-types)
> - [Reading Oracle explain plan output](explain-plan-oracle.md#operations)

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

The query uses the index (`INDEX RANGE SCAN`) and fetches in reverse order (`DECENDING`). The text `NOSORT STOPKEY` means that there is no sort operation needed and that the execution is aborted once the condition (predicate 2) is not met anymore.

```
--------------------------------------------------------------------
| Id | Operation                      | Name    | A-Rows | Buffers |
--------------------------------------------------------------------
|  0 | SELECT STATEMENT               |         |      1 |       4 |
|* 1 |  VIEW                          |         |      1 |       4 |
|* 2 |   WINDOW NOSORT STOPKEY        |         |      1 |       4 |
|  3 |    TABLE ACCESS BY INDEX ROWID | TBL     |      2 |       4 |
|* 4 |     INDEX RANGE SCAN DESCENDING| TBL_IDX |      2 |       2 |
--------------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------

1 - filter("from$_subquery$_002"."rowlimit_$$_rownumber"<=1)
2 - filter(ROW_NUMBER() OVER ( ORDER BY DATE_COLUMN DESC )<=1)
4 - access("A"=12)
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

The execution plan of the first query is fine with both indexes:

```
-------------------------------------------------------------------------
| Id | Operation                           | Name    | A-Rows | Buffers |
-------------------------------------------------------------------------
|  0 | SELECT STATEMENT                    |         |      1 |       3 |
|  1 |  TABLE ACCESS BY INDEX ROWID BATCHED| TBL     |      1 |       3 |
|* 2 |   INDEX RANGE SCAN                  | TBL_IDX |      1 |       2 |
-------------------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
   2 - access("A"=38 AND "B"=1)
```

```
-------------------------------------------------------------------------
| Id | Operation                           | Name    | A-Rows | Buffers |
-------------------------------------------------------------------------
|  0 | SELECT STATEMENT                    |         |      1 |       3 |
|  1 |  TABLE ACCESS BY INDEX ROWID BATCHED| TBL     |      1 |       3 |
|* 2 |   INDEX RANGE SCAN                  | TBL_IDX |      1 |       2 |
-------------------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
   2 - access("B"=1 AND "A"=38)
```

Both indexes can be used with an `INDEX RANGE SCAN` and without any filter predicates.

The second query performs an INDEX SKIP SCAN. From the efficiency perspective, this is better than an `INDEX FULL SCAN` but still worse than an `INDEX RANGE SCAN`.

```
-------------------------------------------------------------------------
| Id | Operation                           | Name    | A-Rows | Buffers |
-------------------------------------------------------------------------
|  0 | SELECT STATEMENT                    |         |      2 |       4 |
|  1 |  TABLE ACCESS BY INDEX ROWID BATCHED| TBL     |      2 |       4 |
|* 2 |   INDEX SKIP SCAN                   | TBL_IDX |      2 |       2 |
-------------------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
   2 - access("B"=1)
       filter("B"=1)
```

The second query can use the index on (b, a) with an `INDEX RANGE SCAN`, however.

```
-------------------------------------------------------------------------
| Id | Operation                           | Name    | A-Rows | Buffers |
-------------------------------------------------------------------------
|  0 | SELECT STATEMENT                    |         |      2 |       4 |
|  1 |  TABLE ACCESS BY INDEX ROWID BATCHED| TBL     |      2 |       4 |
|* 2 |   INDEX RANGE SCAN                  | TBL_IDX |      2 |       2 |
-------------------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
   2 - access("B"=1)
```

Reversing the column order in the index doesn’t affect the first query, yet it improves the performance of the second query.

> **Learn More:**
>
> - [The column order in Multi-Column Indexes](where-clause-the-equals-operator.md#concatenated-indexes)

### Question 4 — LIKE

```
CREATE INDEX tbl_idx ON tbl (text);
```

```
SELECT *
  FROM tbl
 WHERE text LIKE 'TJ%';
```

The execution plan clearly states that it is doing an index range scan. Since the only wild card character is at the very end, the full search term `'TJ'` can be used as [index access predicate](where-clause-searching-for-ranges.md#greater-less-and-between).

```
-------------------------------------------------------------------------
| Id | Operation                           | Name    | A-Rows | Buffers |
-------------------------------------------------------------------------
|  0 | SELECT STATEMENT                    |         |      1 |       4 |
|  1 |  TABLE ACCESS BY INDEX ROWID BATCHED| TBL     |      1 |       4 |
|* 2 |   INDEX RANGE SCAN                  | TBL_IDX |      1 |       3 |
-------------------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
   2 - access("TEXT" LIKE 'TJ%')
       filter("TEXT" LIKE 'TJ%')
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

Both queries use the index, of course. The difference is that the first query doesn’t access the table, so the first query is much faster.

```
----------------------------------------------------------
| Id | Operation            | Name    | A-Rows | Buffers |
----------------------------------------------------------
|  0 | SELECT STATEMENT     |         |      3 |       2 |
|  1 |  SORT GROUP BY NOSORT|         |      3 |       2 |
|* 2 |   INDEX RANGE SCAN   | TBL_IDX |      3 |       2 |
----------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
   2 - access("A"=38)
```

```
------------------------------------------------------------------
| Id | Operation                    | Name    | A-Rows | Buffers |
------------------------------------------------------------------
|  0 | SELECT STATEMENT             |         |      1 |       3 |
|  1 |  SORT GROUP BY NOSORT        |         |      1 |       3 |
|* 2 |   TABLE ACCESS BY INDEX ROWID| TBL     |      1 |       3 |
|* 3 |    INDEX RANGE SCAN          | TBL_IDX |      3 |       1 |
------------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
   2 - filter("B"=1)
   3 - access("A"=38)
```

The database could, theoretically, use a `FAST FULL INDEX SCAN` for the first query, if selecting a large fraction of the table. The second query, using `INDEX RANGE SCAN` and `TABLE ACCESS BY INDEX ROWID` could be faster in that case. However, this case doesn’t apply here because the first query selects a small fraction from the table.

The other border case, if the first query doesn’t return any rows, means that the second query would be as fast as the first.

Besides these border cases, the second query must be considerable slower because every row needs a table access — also for those that are filtered by the new condition. Even if the index has a low [clustering factor](clustering.md#index-filter-predicates-used-intentionally), it is still about twice as many blocks to read.

> **Learn More:**
>
> - [Chapter 5*Clustering Data: The Second Power of Indexing*](clustering.md)
