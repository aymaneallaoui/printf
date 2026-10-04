<!-- Source: https://use-the-index-luke.com/sql/example-schema/sqlbase — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# SQLBase Example Scripts

<sub>Source: https://use-the-index-luke.com/sql/example-schema/sqlbase</sub>

The following scripts were tested with Gupta SQLBase 11.7.

## Contents

1. *[The `where` clause](#sqlbase-example-scripts-for-the-where-clause)*
2. *[The Join Operation](#sqlbase-example-scripts-for-the-join-operation)*


## SQLBase Example Scripts for “The Where Clause”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/sqlbase/where-clause</sub>

### The Equals Operator

#### Surrogate Keys

Creating the `EMPLOYEES` table with 1000 rows.

```
CREATE TABLE employees (
   employee_id   INTEGER       NOT NULL,
   first_name    VARCHAR(200) NOT NULL,
   last_name     VARCHAR(200) NOT NULL,
   date_of_birth DATE                  ,
   phone_number  VARCHAR(200) NOT NULL,
   junk          CHAR(254)             ,
   PRIMARY KEY (employee_id)
);

CREATE UNIQUE INDEX employees_pk ON employees(employee_id);

CREATE TABLE generator_16 (
   n NUMBER NOT NULL,
   PRIMARY KEY (n)
);

CREATE UNIQUE INDEX generator_16_pk ON generator_16 (n);

INSERT INTO generator_16 VALUES (0);
INSERT INTO generator_16 VALUES (1);
INSERT INTO generator_16 VALUES (2);
INSERT INTO generator_16 VALUES (3);
INSERT INTO generator_16 VALUES (4);
INSERT INTO generator_16 VALUES (5);
INSERT INTO generator_16 VALUES (6);
INSERT INTO generator_16 VALUES (7);
INSERT INTO generator_16 VALUES (8);
INSERT INTO generator_16 VALUES (9);
INSERT INTO generator_16 VALUES (10);
INSERT INTO generator_16 VALUES (11);
INSERT INTO generator_16 VALUES (12);
INSERT INTO generator_16 VALUES (13);
INSERT INTO generator_16 VALUES (14);
INSERT INTO generator_16 VALUES (15);

CREATE VIEW generator_256 (n)
AS SELECT ( ( hi.n * 16 ) + lo.n ) AS n
     FROM generator_16 lo, generator_16 hi;

CREATE VIEW generator_4k (n)
AS SELECT ( ( hi.n * 256 ) + lo.n ) AS n
     FROM generator_256 lo, generator_16 hi;

CREATE VIEW generator_64k (n)
AS SELECT ( ( hi.n * 256 ) + lo.n ) AS n
     FROM generator_256 lo, generator_256 hi;

INSERT INTO employees (employee_id,  first_name,
                       last_name,    date_of_birth,
                       phone_number, junk)
SELECT n +1,
       @CHAR(@mod(n, 26)+65)    ||  @CHAR(@mod(@microsecond(@NOW), 26)+97)   || @CHAR(@mod(@minute(@NOW)+n, 26)+97),
       @CHAR(@mod(13+n, 26)+65) ||  @CHAR(@mod(@microsecond(@NOW)+7, 26)+97) || @CHAR(@mod(@minute(@NOW)+n+17, 26)+97),
       SYSDATE - @mod(n+@MICROSECOND(@NOW), 40*365) - 3650,
       @MOD(@MICROSECOND(@NOW)+n, 9000) + 1000,
       'junk'
  FROM generator_4k
WHERE n < 1000;

UPDATE employees
   SET first_name='MARKUS',
       last_name='WINAND'
 WHERE employee_id=123;

UPDATE STATISTICS ON TABLE employees;
UPDATE STATISTICS ON INDEX employees_pk;
```

Note:

- We use 200 characters to be on the safe side regarding SQLBases limit on key lenght (total < 255, including overhead).

#### Concatenated Keys

```
-- add subsidiary_id and update existing records
ALTER TABLE employees ADD subsidiary_id INTEGER;
UPDATE      employees SET subsidiary_id = 30;
ALTER TABLE employees MODIFY subsidiary_id NOT NULL;

-- change the PK
﻿ALTER TABLE employees DROP PRIMARY KEY;
DROP INDEX employees_pk;

CREATE UNIQUE INDEX employees_pk ON employees (employee_id, subsidiary_id);
ALTER TABLE employees PRIMARY KEY (employee_id, subsidiary_id);

-- generate more records (Very Big Company)
INSERT INTO employees (employee_id,  first_name,
                       last_name,    date_of_birth,
                       phone_number, subsidiary_id, junk)
SELECT n +1,
       @CHAR(@mod(n, 26)+65)    ||  @CHAR(@mod(@microsecond(@NOW), 26)+97)   || @CHAR(@mod(@minute(@NOW)+n, 26)+97),
       @CHAR(@mod(13+n, 26)+65) ||  @CHAR(@mod(@microsecond(@NOW)+7, 26)+97) || @CHAR(@mod(@minute(@NOW)+n+17, 26)+97),
       SYSDATE - @mod(n+@MICROSECOND(@NOW), 40*365) - 3650,
       @MOD(@MICROSECOND(@NOW)+n, 9000) + 1000,
       @MOD(@SECOND(@NOW)+n/9000*29,29),
       'junk'
  FROM generator_64k
WHERE n < 9000;

UPDATE STATISTICS ON TABLE employees;
UPDATE STATISTICS ON INDEX employees_pk;
```

```
CREATE INDEX emp_sub_id ON employees (subsidiary_id);
```

Although that gives decent performance, it’s better to use the index that supports the primary key:

```
-- use tmp index to support the PK
ALTER TABLE employees DROP PRIMARY KEY;
DROP INDEX employees_pk;

CREATE UNIQUE INDEX employees_pk ON employees (subsidiary_id, employee_id);
ALTER TABLE employees PRIMARY KEY (subsidiary_id, employee_id);

DROP INDEX emp_sub_id;

UPDATE STATISTICS ON INDEX employees_pk;
```

### Functions

```
CREATE INDEX emp_name_up ON employees (@upper(last_name));
```


## SQLBase Example Scripts for “The Join Operation”

<sub>Source: https://use-the-index-luke.com/sql/example-schema/sqlbase/join</sub>

This section contains the `create` and `insert` code to run the examples from [Chapter 4*The Join Operation*](join.md) in an Gupta/Unify SQLBase database.

```
CREATE TABLE sales (
  sale_id       INTEGER NOT NULL,
  employee_id   INTEGER NOT NULL,
  subsidiary_id INTEGER NOT NULL,
  sale_date     DATE   NOT NULL,
  eur_value     DECIMAL(13,2) NOT NULL,
  product_id    INTEGER NOT NULL,
  quantity      INTEGER NOT NULL,
  junk          CHAR(200),
  PRIMARY KEY (sale_id),
  FOREIGN KEY (subsidiary_id, employee_id)
   REFERENCES employees
);

﻿CREATE UNIQUE INDEX sales_pk ON sales (sale_id);

INSERT INTO sales (sale_id
                 , subsidiary_id, employee_id
                 , sale_date, eur_value
                 , product_id, quantity
                 , junk)
       SELECT gen.n *2000*30 + e.employee_id * 30 + e.subsidiary_id AS sale_id
            , e.subsidiary_id, e.employee_id
            , SYSDATE - @mod(n+@MICROSECOND(@NOW), 10*365) AS  sale_date
            , (@MOD(@MICROSECOND(@NOW)+n, 9000) + 1000)/100 AS eur_value
            , @MOD(@SECOND(@NOW)+n/9000*29,29) AS product_id
            , @MOD(@SECOND(@NOW)+n/9000*26,5) AS quantity
            , 'junk'
         FROM employees e
          , generator_4k gen
        WHERE @MOD(employee_id, 7) = 4
              AND gen.n < 1800
          AND gen.n < employee_id / 5 ;
```

Notes:

- The rows are NOT inserted chronologically
- Only a small fraction of employees have sales at all.
