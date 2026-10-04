<!-- Source: https://use-the-index-luke.com/sql/where-clause/obfuscation — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# Obfuscated Conditions

<sub>Source: https://use-the-index-luke.com/sql/where-clause/obfuscation</sub>

The following sections demonstrate some popular methods for obfuscating conditions. Obfuscated conditions are `where` clauses that are phrased in a way that prevents proper index usage. This section is a collection of anti-patterns every developer should know about and avoid.

## Contents

1. *[Dates](#date-types)* — Pay special attention to `DATE` types
2. *[Numeric Strings](#numeric-strings)* — Don’t mix types
3. *[Combining Columns](#combining-columns)* — use redundant `where` clauses
4. *[Smart Logic](#smart-logic)* — The smartest way to make SQL slow
5. *[Math](#math)* — Databases don’t solve equations


## Date Types

<sub>Source: https://use-the-index-luke.com/sql/where-clause/obfuscation/dates</sub>

Most obfuscations involve `DATE` types. The Oracle database is particularly vulnerable in this respect because it has only one `DATE` type that always includes a time component as well.

It has become common practice to use the `TRUNC` function to remove the time component. In truth, it does not remove the time but instead sets it to midnight because the Oracle database has no pure `DATE` type. To disregard the time component for a search you can use the `TRUNC` function on both sides of the comparison—e.g., to search for yesterday’s sales:

```
SELECT ...
  FROM sales
 WHERE TRUNC(sale_date) = TRUNC(sysdate - INTERVAL '1' DAY)
```

It is a perfectly valid and correct statement but it cannot properly make use of an index on `SALE_DATE`. It is as explained in [“*Case-Insensitive Search Using `UPPER` or `LOWER`*”](where-clause-functions.md#case-insensitive-search-using-upper-or-lower); `TRUNC(sale_date)` is something entirely different from `SALE_DATE`—functions are black boxes to the database.

There is a rather simple solution for this problem: a [function-based index](where-clause-functions.md).

```
CREATE INDEX index_name
          ON sales (TRUNC(sale_date))
```

But then you must always use `TRUNC(sale_date)` in the `where` clause. If you use it inconsistently—sometimes with, sometimes without `TRUNC`—then you need two indexes!

The problem also occurs with databases that have a pure date type if you search for a longer period as shown in the following MySQL query:

```
SELECT ...
  FROM sales
 WHERE DATE_FORMAT(sale_date, "%Y-%M")
     = DATE_FORMAT(now()    , "%Y-%M")
```

The query uses a date format that only contains year and month: again, this is an absolutely correct query that has the same problem as before. However the solution from above does not apply to MySQL prior to version 5.7, because MySQL didn’t support function-based indexing before that version.

The alternative is to use an explicit range condition. This is a generic solution that works for all databases:

```
SELECT ...
  FROM sales
 WHERE sale_date BETWEEN quarter_begin(?)
                     AND quarter_end(?)
```

If you have done your homework, you probably recognize the pattern from the [exercise about all employees who are 42 years old](where-clause-functions.md#user-defined-functions).

A straight index on `SALE_DATE` is enough to optimize this query. The functions `QUARTER_BEGIN` and `QUARTER_END` compute the boundary dates. The calculation can become a little complex because the [`between` operator always includes the boundary values](where-clause-searching-for-ranges.md#greater-less-and-between). The `QUARTER_END` function must therefore return a time stamp just before the first day of the next quarter if the `SALE_DATE` has a time component. This logic can be hidden in the function.

The following examples show implementations of the functions `QUARTER_BEGIN` and `QUARTER_END` for various databases.

#### Db2 (LUW)

```
CREATE FUNCTION quarter_begin(dt TIMESTAMP)
RETURNS TIMESTAMP
RETURN TRUNC(dt, 'Q')
```

```
CREATE FUNCTION quarter_end(dt TIMESTAMP)
RETURNS TIMESTAMP
RETURN TRUNC(dt, 'Q') + 3 MONTHS - 1 SECOND
```

#### MySQL

```
CREATE FUNCTION quarter_begin(dt DATETIME)
RETURNS DATETIME DETERMINISTIC
RETURN CONVERT
       (
         CONCAT
         ( CONVERT(YEAR(dt),CHAR(4))
         , '-'
         , CONVERT(QUARTER(dt)*3-2,CHAR(2))
         , '-01'
         )
       , datetime
       )
```

```
CREATE FUNCTION quarter_end(dt DATETIME)
RETURNS DATETIME DETERMINISTIC
RETURN DATE_ADD
       ( DATE_ADD ( quarter_begin(dt), INTERVAL 3 MONTH )
       , INTERVAL -1 MICROSECOND)
```

#### Oracle

```
CREATE FUNCTION quarter_begin(dt IN DATE)
RETURN DATE
AS
BEGIN
   RETURN TRUNC(dt, 'Q');
END
```

```
CREATE FUNCTION quarter_end(dt IN DATE)
RETURN DATE
AS
BEGIN
   -- the Oracle DATE type has seconds resolution
   -- subtract one second from the first
   -- day of the following quarter
   RETURN TRUNC(ADD_MONTHS(dt, +3), 'Q')
        - (1/(24*60*60));
END
```

#### PostgreSQL

```
CREATE FUNCTION quarter_begin(dt timestamp with time zone)
RETURNS timestamp with time zone AS $$
BEGIN
    RETURN date_trunc('quarter', dt);
END;
$$ LANGUAGE plpgsql
```

```
CREATE FUNCTION quarter_end(dt timestamp with time zone)
RETURNS timestamp with time zone AS $$
BEGIN
   RETURN   date_trunc('quarter', dt)
          + interval '3 month'
          - interval '1 microsecond';
END;
$$ LANGUAGE plpgsql
```

#### SQL Server

```
CREATE FUNCTION quarter_begin (@dt DATETIME )
RETURNS DATETIME
BEGIN
  RETURN DATEADD (qq, DATEDIFF (qq, 0, @dt), 0)
END
```

```
CREATE FUNCTION quarter_end (@dt DATETIME )
RETURNS DATETIME
BEGIN
  RETURN DATEADD
         ( ms
         , -3
         , DATEADD(mm, 3, dbo.quarter_begin(@dt))
         );
END
```

You can use similar auxiliary functions for other periods—most of them will be less complex than the examples above, especially when using greater than or equal to (`>=`) and less than (`<`) conditions instead of the `between` operator. Of course you could calculate the boundary dates in your application if you wish.

> **Tip:**
>
> Write queries for continuous periods as explicit range condition. Do this even for a single day—e.g., for the Oracle database:
>
> ```
>     sale_date >= TRUNC(sysdate)
> AND sale_date <  TRUNC(sysdate + INTERVAL '1' DAY)
> ```

Another common obfuscation is to compare dates as strings as shown in the following PostgreSQL example:

```
SELECT ...
  FROM sales
 WHERE TO_CHAR(sale_date, 'YYYY-MM-DD') = '1970-01-01'
```

The problem is, again, converting `SALE_DATE`. Such conditions are often created in the belief that you cannot pass different types than numbers and strings to the database. [Bind parameters](where-clause-bind-parameters.md), however, support all data types. That means you can for example use a `java.util.Date` object as bind parameter. This is yet another benefit of bind parameters.

If you cannot do that, you just have to convert the search term instead of the table column:

```
SELECT ...
  FROM sales
 WHERE sale_date = TO_DATE('1970-01-01', 'YYYY-MM-DD')
```

This query can use a straight index on `SALE_DATE`. Moreover it converts the input string only once. The previous statement must convert all dates stored in the table before it can compare them against the search term.

Whatever change you make—using a bind parameter or converting the other side of the comparison—you can easily introduce a bug if `SALE_DATE` has a time component. You must use an explicit range condition in that case:

```
SELECT ...
  FROM sales
 WHERE sale_date >= TO_DATE('1970-01-01', 'YYYY-MM-DD')
   AND sale_date <  TO_DATE('1970-01-01', 'YYYY-MM-DD')
                  + INTERVAL '1' DAY
```

Always consider using an explicit range condition when comparing dates.

> **Sidebar — LIKE on Date Types**
>
> The following obfuscation is particularly tricky:
>
> ```
> sale_date LIKE SYSDATE
> ```
>
> It does not look like an obfuscation at first glance because it does not use any functions.
>
> The `LIKE` operator, however, enforces a string comparison. Depending on the database, that might yield an error or cause an implicit type conversion on both sides. The “Predicate Information” section of the execution plan shows what the Oracle database does:
>
> ```
> filter( INTERNAL_FUNCTION(SALE_DATE)
>    LIKE TO_CHAR(SYSDATE@!))
> ```
>
> The function [`INTERNAL_FUNCTION`](https://tanelpoder.com/2013/01/16/what-the-heck-is-the-internal_function-in-execution-plan-predicate-section/) converts the type of the `SALE_DATE` column. As a side effect it also prevents using a straight index on `DATE_COLUMN` *just as any other function would*.


## Numeric Strings

<sub>Source: https://use-the-index-luke.com/sql/where-clause/obfuscation/numeric-strings</sub>

Numeric strings are numbers that are stored in text columns. Although it is a very bad practice, it does not automatically render an index useless if you consistently treat it as string:

```
SELECT ...
  FROM ...
 WHERE numeric_string = '42'
```

Of course this statement can use an index on `NUMERIC_STRING`. If you compare it using a number, however, the database can no longer use this condition as an [access predicate](where-clause-searching-for-ranges.md#greater-less-and-between).

```
SELECT ...
  FROM ...
 WHERE numeric_string = 42
```

Note the missing quotes. Although some database yield an error (e.g. PostgreSQL) many databases just add an implicit type conversion.

```
SELECT ...
  FROM ...
 WHERE CAST(numeric_string AS INT) = 42
```

It is the same problem as before. An index on `NUMERIC_STRING` cannot be used due to the function call. The solution is also the same as before: do not convert the table column, instead convert the search term.

```
SELECT ...
  FROM ...
 WHERE numeric_string = CAST(42 AS VARCHAR(10))
```

You might wonder why the database does not do it this way automatically? I think it is because converting a string to a number always gives an unambiguous result. This is not true the other way around. A number, formatted as text, can contain spaces, punctation, and leading zeros. A single value can be written in many ways:

```
42
042
0042
00042
...
```

The database cannot know the number format used in the `NUMERIC_STRING` column so it does it the other way around: the database converts the strings to numbers—this is an unambiguous transformation.

The `CAST AS VARCHAR` expression returns only one string representation of the number. It will therefore only match the first of above listed strings. If we use `CAST AS INT`, it matches all of them. That means there is not only a performance difference between the two variants but also a semantic difference!

Using numeric strings is generally troublesome: most importantly it causes performance problems due to the implicit conversion and also introduces a risk of running into conversion errors due to invalid numbers. Even the most trivial query that does not use any functions in the `where` clause can cause an abort with a conversion error if there is just one invalid number stored in the table.

> **Tip:**
>
> Use numeric types to store numbers.

Note that the problem does not exist the other way around:

```
SELECT ...
  FROM ...
 WHERE numeric_number = '42'
```

The database will consistently transform the string into a number. It does not apply a function on the potentially indexed column: a regular index will therefore work. Nevertheless it is possible to do a manual conversion the wrong way:

```
SELECT ...
  FROM ...
 WHERE TO_CHAR(numeric_number) = '42'
```


## Combining Columns

<sub>Source: https://use-the-index-luke.com/sql/where-clause/obfuscation/concatenation</sub>

This section is about a popular obfuscation that affects [concatenated indexes](where-clause-the-equals-operator.md#concatenated-indexes).

The first example is again about [date and time](#date-types) types but the other way around. The following MySQL query combines a date and a time column to apply a range filter on both of them.

```
SELECT ...
  FROM ...
 WHERE ADDTIME(date_column, time_column)
     > DATE_ADD(now(), INTERVAL -1 DAY)
```

It selects all records from the last 24 hours. The query cannot use a concatenated index on (`DATE_COLUMN`, `TIME_COLUMN`) properly because the search is not done on the indexed columns but on derived data.

You can avoid this problem by using a data type that has both a date and time component (e.g., MySQL `DATETIME`). You can then use this column without a function call:

```
SELECT ...
  FROM ...
 WHERE datetime_column
     > DATE_ADD(now(), INTERVAL -1 DAY)
```

Unfortunately it is often not possible to change the table when facing this problem.

The next option is a [function-based index](where-clause-functions.md#case-insensitive-search-using-upper-or-lower) if the database supports it—although this has all the drawbacks [discussed before](#date-types). When using MySQL, function-based indexes are not an option anyway.

It is still possible to write the query so that the database can use a concatenated index on `DATE_COLUMN`, `TIME_COLUMN` with an [access predicate](where-clause-searching-for-ranges.md#greater-less-and-between)—at least partially. For that, we add an extra condition on the `DATE_COLUMN`.

```
 WHERE ADDTIME(date_column, time_column)
     > DATE_ADD(now(), INTERVAL -1 DAY)
   AND date_column
    >= DATE(DATE_ADD(now(), INTERVAL -1 DAY))
```

The new condition is absolutely redundant but it is a straight filter on `DATE_COLUMN` that can be used as access predicate. Even though this technique is not perfect, it is usually a good enough approximation.

> **Tip:**
>
> Use a redundant condition on the most significant column when a range condition combines multiple columns.
>
> For PostgreSQL, it’s preferable to use the [row values syntax](partial-results.md#paging-through-results).

You can also use this technique when storing date and time in text columns, but you have to use date and time formats that yields a chronological order when sorted lexically—e.g., as suggested by [ISO 8601](https://en.wikipedia.org/wiki/ISO_8601) (`YYYY-MM-DD HH:MM:SS`). The following example uses the Oracle database’s `TO_CHAR` function for that purpose:

```
SELECT ...
  FROM ...
 WHERE date_string || time_string
     > TO_CHAR(sysdate - 1, 'YYYY-MM-DD HH24:MI:SS')
   AND date_string
    >= TO_CHAR(sysdate - 1, 'YYYY-MM-DD')
```

We will face the problem of applying a range condition over multiple columns again in the section entitled [“*Paging Through Results*”](partial-results.md#paging-through-results). We’ll also use the same approximation method to mitigate it.

Sometimes we have the reverse case and might want to obfuscate a condition intentionally so it cannot be used anymore as access predicate. We already looked at that problem when discussing the effects of [bind parameters](where-clause-bind-parameters.md) on `LIKE` conditions. Consider the following example:

```
SELECT last_name, first_name, employee_id
  FROM employees
 WHERE subsidiary_id = ?
   AND last_name LIKE ?
```

Assuming there is an index on `SUBSIDIARY_ID` and another one on `LAST_NAME`, which one is better for this query?

Without knowing the wildcard’s position in the search term, it is impossible to give a qualified answer. The optimizer has no other choice than to “guess”. If *you know* that there is always a leading wild card, you can obfuscate the `LIKE` condition intentionally so that the optimizer can no longer consider the index on `LAST_NAME`.

```
SELECT last_name, first_name, employee_id
  FROM employees
 WHERE subsidiary_id = ?
   AND last_name || '' LIKE ?
```

It is enough to append an empty string to the `LAST_NAME` column. This is, however, an option of last resort. Only do it when absolutely necessary.


## Smart Logic

<sub>Source: https://use-the-index-luke.com/sql/where-clause/obfuscation/smart-logic</sub>

One of the key features of SQL databases is their support for ad-hoc queries: new queries can be executed at any time. This is only possible because the [query optimizer](where-clause-the-equals-operator.md#slow-indexes-part-ii) (query planner) works at runtime; it analyzes each statement when received and generates a reasonable execution plan immediately. The overhead introduced by runtime optimization can be minimized with [bind parameters](where-clause-bind-parameters.md).

The gist of that recap is that databases are optimized for dynamic SQL—so use it if you need it.

Nevertheless there is a widely used practice that avoids dynamic SQL in favor of static SQL—often because of the “[dynamic SQL is slow](myth-directory.md#dynamic-sql-is-slow)” myth. This practice does more harm than good if the database uses a shared execution plan cache like Db2 (LUW), the Oracle database, or SQL Server.

For the sake of demonstration, imagine an application that queries the `EMPLOYEES` table. The application allows searching for subsidiary id, employee id and last name (case-insensitive) in any combination. It is still possible to write a single query that covers all cases by using “smart” logic.

```
SELECT first_name, last_name, subsidiary_id, employee_id
  FROM employees
 WHERE ( subsidiary_id    = :sub_id OR :sub_id IS NULL )
   AND ( employee_id      = :emp_id OR :emp_id IS NULL )
   AND ( UPPER(last_name) = :name   OR :name   IS NULL )
```

The query uses [named bind variables](where-clause-bind-parameters.md) for better readability. All possible filter expressions are statically coded in the statement. Whenever a filter isn’t needed, you just use `NULL` instead of a search term: it disables the condition via the `OR` logic.

It is a perfectly reasonable SQL statement. The use of `NULL` is even in line with its [definition according to the three-valued logic of SQL](where-clause-null.md). Nevertheless it is one of the *worst performance anti-patterns* of all.

The database cannot optimize the execution plan for a particular filter because any of them could be canceled out at runtime. The database needs to prepare for the worst case—if all filters are disabled:

```
----------------------------------------------------
| Id | Operation         | Name      | Rows | Cost |
----------------------------------------------------
|  0 | SELECT STATEMENT  |           |    2 |  478 |
|* 1 |  TABLE ACCESS FULL| EMPLOYEES |    2 |  478 |
----------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
1 - filter((:NAME   IS NULL OR UPPER("LAST_NAME")=:NAME)
       AND (:EMP_ID IS NULL OR "EMPLOYEE_ID"=:EMP_ID)
       AND (:SUB_ID IS NULL OR "SUBSIDIARY_ID"=:SUB_ID))
```

As a consequence, the database uses a full table scan *even if there is an index for each column*.

It is not that the database cannot resolve the “smart” logic. It creates the generic execution plan due to the use of bind parameters so it can be cached and re-used with other values later on. If we do not use [bind parameters](where-clause-bind-parameters.md) but write the actual values in the SQL statement, the optimizer selects the proper index for the active filter:

```
SELECT first_name, last_name, subsidiary_id, employee_id
  FROM employees
 WHERE( subsidiary_id    = NULL     OR NULL IS NULL )
   AND( employee_id      = NULL     OR NULL IS NULL )
   AND( UPPER(last_name) = 'WINAND' OR 'WINAND' IS NULL )
```

```
---------------------------------------------------------------
|Id | Operation                   | Name        | Rows | Cost |
---------------------------------------------------------------
| 0 | SELECT STATEMENT            |             |    1 |    2 |
| 1 |  TABLE ACCESS BY INDEX ROWID| EMPLOYEES   |    1 |    2 |
|*2 |   INDEX RANGE SCAN          | EMP_UP_NAME |    1 |    1 |
---------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------
  2 - access(UPPER("LAST_NAME")='WINAND')
```

This, however, is no solution. It just proves that the database can resolve these conditions.

> **Warning:**
>
> Using literal values makes your application vulnerable to [SQL injection](https://en.wikipedia.org/wiki/SQL_injection) attacks and can cause performance problems due to increased optimization overhead.

The obvious solution for dynamic queries is dynamic SQL. According to the [KISS principle](https://en.wikipedia.org/wiki/KISS_principle), just tell the database what you need right now—and nothing else.

```
SELECT first_name, last_name, subsidiary_id, employee_id
  FROM employees
 WHERE UPPER(last_name) = :name
```

Note that the query uses a bind parameter.

> **Tip:**
>
> Use dynamic SQL if you need dynamic `where` clauses.
>
> Still use bind parameters when generating dynamic SQL—otherwise the “[dynamic SQL is slow](myth-directory.md#dynamic-sql-is-slow)” myth comes true.

The problem described in this section is widespread. All databases that use a shared execution plan cache have a feature to cope with it—often introducing new problems and bugs.

#### Db2 (LUW)

Db2 uses a shared execution plan cache and is fully exposed to the problem described in this section.

Db2 allows to specify the [re-optimization approach](https://www.ibm.com/docs/en/db2/11.5.x?topic=commands-bind) using the `REOPT` hint. The default is `NONE`, which produces a generic execution plan and suffers from the problem described above. `REOPT(ALWAYS)` will tell the optimizer to always peek the actual bind variables to produce the best plan for each execution. That is effectively turning off execution plan caching for that statement.

The last option is `REOPT(ONCE)` which will peek the bind parameters for the first execution only. The problem with this approach is its nondeterministic behavior: the values from the first execution affect all executions. The execution plan can change whenever the database is restarted or, less predictably, the cached plan expires and the optimizer recreates it using different values the next time the statement is executed.

#### MySQL

MySQL does not suffer from this particular problem because it has no execution plan cache at all . A [feature request from 2009](https://bugs.mysql.com/bug.php?id=42808) discusses the impact of execution plan caching. It seems that MySQL’s optimizer is simple enough so that execution plan caching does not pay off.

#### Oracle

The Oracle database uses a shared execution plan cache (“SQL area”) and is fully exposed to the problem described in this section.

Oracle introduced the so-called *bind peeking* with release 9*i*. Bind peeking enables the optimizer to use the actual bind values of the first execution when preparing an execution plan. The problem with this approach is its nondeterministic behavior: the values from the first execution affect all executions. The execution plan can change whenever the database is restarted or, less predictably, the cached plan expires and the optimizer recreates it using different values the next time the statement is executed.

Release 11*g* introduced *adaptive cursor sharing* to further improve the situation. This feature allows the database to cache multiple execution plans for the same SQL statement. Further, the optimizer peeks the bind parameters and stores their estimated selectivity along with the execution plan. When the cache is subsequently accessed, the selectivity of the current bind values must fall within the selectivity ranges of a cached execution plan to be reused. Otherwise the optimizer creates a new execution plan and compares it against the already cached execution plans for this query. If there is already such an execution plan, the database replaces it with a new execution plan that also covers the selectivity estimates of the current bind values. If not, it caches a new execution plan variant for this query — along with the selectivity estimates, of course.

#### PostgreSQL

The PostgreSQL query plan cache works for open statements only—that is as long as you keep the `PreparedStatement` open. The above described problem occurs only when re-using a statement handle. Note that PostgreSQL’s JDBC driver enables the cache after the fifth execution only. See also: [Planning with Actual Bind Values](https://use-the-index-luke.com/sql/explain-plan/postgres/concrete-planning).

#### SQL Server

SQL Server uses so-called *parameter sniffing*. Parameter sniffing enables the optimizer to use the actual bind values of the first execution during parsing. The problem with this approach is its nondeterministic behavior: the values from the first execution affect all executions. The execution plan can change whenever the database is restarted or, less predictably, the cached plan expires and the optimizer recreates it using different values the next time the statement is executed.

SQL Server provides a query hints to gain more control over parameter sniffing and recompiling. The [query hint](https://learn.microsoft.com/en-us/sql/t-sql/queries/hints-transact-sql-query?view=sql-server-ver16) `RECOMPILE` bypasses the plan cache for a selected statement. `OPTIMIZE FOR` allows the specification of actual parameter values that are used for optimization only. Finally, you can provide an entire execution plan with the `USE PLAN` hint.

However, these features come a long way as there were several bugs and surprising behavior in special cases. The description of these is way beyond the scope of this book but luckily [Erland Sommarskog maintains all the relevant information up to SQL Server 2022](https://www.sommarskog.se/dyn-search-2008.html).

Although heuristic methods can improve the “smart logic” problem to a certain extent, they were actually built to deal with the problems of bind parameter in connection with column histograms and `LIKE` expressions.

The most reliable method for arriving at the best execution plan is to avoid unnecessary filters in the SQL statement.

> **See Also:**
>
> [Using Bind-Variables - Examples](where-clause-bind-parameters.md)
>
> [Building DynamicSQL using ORM Tools - Examples](myth-directory.md#dynamic-sql-is-slow)


## Math

<sub>Source: https://use-the-index-luke.com/sql/where-clause/obfuscation/math</sub>

There is one more class of obfuscations that is smart and prevents proper index usage. Instead of using logic expressions it is using a calculation.

Consider the following statement. Can it use an index on `NUMERIC_NUMBER`?

```
SELECT numeric_number
  FROM table_name
 WHERE numeric_number - 1000 > ?
```

Similarly, can the following statement use an index on `A` and `B`—you choose the order?

```
SELECT a, b
  FROM table_name
 WHERE 3*a + 5 = b
```

Let’s put these questions into a different perspective; if you were developing an SQL database, would you add an equation solver? Most database vendors just say “No!” and thus, neither of the two examples uses the index.

You can even use math to obfuscate a condition intentionally—[as we did it previously for the full text `LIKE` search](#combining-columns). It is enough to add zero, for example:

```
SELECT numeric_number
  FROM table_name
 WHERE numeric_number + 0 = ?
```

Nevertheless we can index these expressions with a [function-based index](where-clause-functions.md#case-insensitive-search-using-upper-or-lower) if we use calculations in a smart way and transform the `where` clause like an equation:

```
SELECT a, b
  FROM table_name
 WHERE 3*a - b = -5
```

We just moved the table references to the one side and the constants to the other. We can then create a function-based index for the left hand side of the equation:

```
CREATE INDEX math ON table_name (3*a - b)
```
