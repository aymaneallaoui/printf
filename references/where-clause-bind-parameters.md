<!-- Source: https://use-the-index-luke.com/sql/where-clause/bind-parameters — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# Parameterized Queries

<sub>Source: https://use-the-index-luke.com/sql/where-clause/bind-parameters</sub>

This section covers a topic that is skipped in most SQL textbooks: *parameterized queries* and *bind parameters*.

Bind parameters—also called dynamic parameters or bind variables—are an alternative way to pass data to the database. Instead of putting the values directly into the SQL statement, you just use a placeholder like `?`, `:name` or `@name` and provide the actual values using a separate API call.

There is nothing bad about writing values directly into ad-hoc statements; there are, however, two good reasons to use bind parameters in programs:

Security
:   Bind variables are the best way to prevent [SQL injection](https://en.wikipedia.org/wiki/SQL_injection).

Performance
:   Databases with an execution plan cache like SQL Server and the Oracle database can reuse an execution plan when executing the same statement multiple times. It saves effort in rebuilding the execution plan but works only if the SQL statement is *exactly* the same. If you put different values into the SQL statement, the database handles it like a different statement and recreates the execution plan.

    When using bind parameters you do not write the actual values but instead insert placeholders into the SQL statement. That way the statements do not change when executing them with different values.

Naturally there are exceptions, for example if the affected data volume depends on the actual values:

```
SELECT first_name, last_name
  FROM employees
 WHERE subsidiary_id = 20
```

```
99 rows selected.

----------------------------------------------------------------
|Id | Operation                   | Name         | Rows | Cost |
----------------------------------------------------------------
| 0 | SELECT STATEMENT            |              |   99 |   70 |
| 1 |  TABLE ACCESS BY INDEX ROWID| EMPLOYEES    |   99 |   70 |
|*2 |   INDEX RANGE SCAN          | EMPLOYEES_PK |   99 |    2 |
----------------------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------

   2 - access("SUBSIDIARY_ID"=20)
```

An index lookup delivers the best performance for small subsidiaries, but a `TABLE ACCESS FULL` can outperform the index for large subsidiaries:

```
SELECT first_name, last_name
  FROM employees
 WHERE subsidiary_id = 30
```

```
1000 rows selected.

----------------------------------------------------
| Id | Operation         | Name      | Rows | Cost |
----------------------------------------------------
|  0 | SELECT STATEMENT  |           | 1000 |  478 |
|* 1 |  TABLE ACCESS FULL| EMPLOYEES | 1000 |  478 |
----------------------------------------------------

Predicate Information (identified by operation id):
---------------------------------------------------

   1 - filter("SUBSIDIARY_ID"=30)
```

In this case, the histogram on `SUBSIDIARY_ID` fulfills its purpose. The optimizer uses it to determine the frequency of the subsidiary ID mentioned in the SQL query. Consequently it gets two different row count estimates for both queries.

The subsequent cost calculation will therefore result in two different cost values. When the optimizer finally selects an execution plan it takes the plan with the lowest cost value. For the smaller subsidiary, it is the one using the index.

The cost of the `TABLE ACCESS BY INDEX ROWID` operation is highly sensitive to the row count estimate. Selecting ten times as many rows will elevate the cost value by that factor. The overall cost using the index is then even higher than a full table scan. The optimizer will therefore select the other execution plan for the bigger subsidiary.

When using bind parameters, the optimizer has no concrete values available to determine their frequency. It then just assumes an equal distribution and always gets the same row count estimates and cost values. In the end, it will always select the same execution plan.

> **Tip:**
>
> Column histograms are most useful if the values are not uniformly distributed.
>
> For columns with uniform distribution, it is often sufficient to divide the number of distinct values by the number of rows in the table. This method also works when using bind parameters.

If we compare the optimizer to a compiler, bind variables are like program variables, but if you write the values directly into the statement they are more like constants. The database can use the values from the SQL statement during optimization just like a compiler can evaluate constant expressions during compilation. Bind parameters are, put simply, not visible to the optimizer just as the runtime values of variables are not known to the compiler.

From this perspective, it is a little bit paradoxical that bind parameters can improve performance if not using bind parameters enables the optimizer to always opt for the best execution plan. But the question is at what price? Generating and evaluating all execution plan variants is a huge effort that does not pay off if you get the same result in the end anyway.

> **Tip:**
>
> Not using bind parameters is like recompiling a program every time.

Deciding to build a specialized or generic execution plan presents a dilemma for the database. Either effort is taken to evaluate all possible plan variants for each execution in order to always get the best execution plan or the optimization overhead is saved and a cached execution plan is used whenever possible—accepting the risk of using a suboptimal execution plan. The quandary is that the database does not know if the full optimization cycle delivers a different execution plan without actually doing the full optimization. Database vendors try to solve this dilemma with heuristic methods—but with very limited success.

As the developer, you can use bind parameters deliberately to help resolve this dilemma. That is, you should always use bind parameters except for values that *shall* influence the execution plan.

Unevenly distributed status codes like “todo” and “done” are a good example. The number of “done” entries often exceeds the “todo” records by an order of magnitude. Using an index only makes sense when searching for “todo” entries in that case. Partitioning is another example—that is, if you split tables and indexes across several storage areas. The actual values can then influence which partitions have to be scanned. The performance of `LIKE` queries can suffer from bind parameters as well as we will see in the [next section](where-clause-searching-for-ranges.md#indexing-like-filters).

> **Tip:**
>
> In all reality, there are only a few cases in which the actual values affect the execution plan. You should therefore use bind parameters if in doubt—just to prevent SQL injections.

The following code snippets show how to use bind parameters in various programming languages.

### C#

Without bind parameters:

```
int subsidiary_id;
SqlCommand cmd = new SqlCommand(
                   "select first_name, last_name"
                 + "  from employees"
                 + " where subsidiary_id = " + subsidiary_id
                 , connection);
```

Using a bind parameter:

```
int subsidiary_id;
SqlCommand cmd =
       new SqlCommand(
                      "select first_name, last_name"
                    + "  from employees"
                    + " where subsidiary_id = @subsidiary_id
                    , connection);
cmd.Parameters.AddWithValue("@subsidiary_id", subsidiary_id);
```

See also: [`SqlParameterCollection`](https://learn.microsoft.com/en-us/dotnet/api/system.data.sqlclient.sqlparametercollection?view=netframework-4.8.1&viewFallbackFrom=dotnet-plat-ext-5.0) class documentation.

### Java

Without bind parameters:

```
int subsidiary_id;
Statement command = connection.createStatement(
                    "select first_name, last_name"
                  + "  from employees"
                  + " where subsidiary_id = " + subsidiary_id
                  );
```

Using a bind parameter:

```
int subsidiary_id;
PreparedStatement command = connection.prepareStatement(
                    "select first_name, last_name"
                  + "  from employees"
                  + " where subsidiary_id = ?"
                  );
command.setInt(1, subsidiary_id);
```

See also: [`PreparedStatement`](https://docs.oracle.com/javase/8/docs/api/java/sql/PreparedStatement.html) class documentation.

### Perl

Without bind parameters:

```
my $subsidiary_id;
my $sth = $dbh->prepare(
                  "select first_name, last_name"
                . "  from employees"
                . " where subsidiary_id = $subsidiary_id"
                );
$sth->execute();
```

Using a bind parameter:

```
my $subsidiary_id;
my $sth = $dbh->prepare(
                  "select first_name, last_name"
                . "  from employees"
                . " where subsidiary_id = ?"
                );
$sth->execute($subsidiary_id);
```

See: [Programming the Perl DBI](https://docstore.mik.ua/orelly/linux/dbi/ch05_03.htm).

### PHP

Using MySQL, without bind parameters:

```
$mysqli->query("select first_name, last_name"
             . "  from employees"
             . " where subsidiary_id = " . $subsidiary_id);
```

Using a bind parameter:

```
if ($stmt = $mysqli->prepare("select first_name, last_name"
                           . "  from employees"
                           . " where subsidiary_id = ?"))
{
   $stmt->bind_param("i", $subsidiary_id);
   $stmt->execute();
} else {
  /* handle SQL error */
}
```

See also: [`mysqli_stmt::bind_param`](https://www.php.net/manual/en/mysqli-stmt.bind-param.php) class documentation and [“Prepared statements and stored procedures” in the PDO documentation](https://www.php.net/manual/en/pdo.prepared-statements.php).

### Ruby

Without bind parameters:

```
dbh.execute("select first_name, last_name"
          + "  from employees"
          + " where subsidiary_id = #{subsidiary_id}");
```

Using a bind parameter:

```
dbh.prepare("select first_name, last_name"
          + "  from employees"
          + " where subsidiary_id = ?");
dbh.execute(subsidiary_id);
```

See also: [“Quoting, Placeholders, and Parameter Binding” in the Ruby DBI Tutorial](https://web.archive.org/web/20190228233411/http://www.kitebird.com/articles/ruby-dbi.html#TOC_8).

> **See Also:**
>
> [Examples using ORM Tools](myth-directory.md#dynamic-sql-is-slow)

The question mark (`?`) is the only placeholder character that the SQL standard defines. Question marks are positional parameters. That means the question marks are numbered from left to right. To bind a value to a particular question mark, you have to specify its number. That can, however, be very impractical because the numbering changes when adding or removing placeholders. Many databases offer a proprietary extension for named parameters to solve this problem—e.g., using an “at” symbol (`@name`) or a colon (`:name`).

> **Note:**
>
> Bind parameters cannot change the structure of an SQL statement.
>
> That means you cannot use bind parameters for table or column names. The following bind parameters do not work:
>
> ```
> String sql = prepare("SELECT * FROM ? WHERE ?");
>
> sql.execute('employees', 'employee_id = 1');
> ```
>
> If you need to change the structure of an SQL statement during runtime, use [dynamic SQL](myth-directory.md#dynamic-sql-is-slow).

> **Sidebar — Cursor Sharing and Forced Parameterization**
>
> The more complex the optimizer and the SQL query become, the more important execution plan caching becomes. The SQL Server and Oracle databases have features to automatically replace the literal values in a SQL string with bind parameters. These features are called `CURSOR_SHARING` (Oracle) or *forced parameterization* (SQL Server).
>
> Both features are workarounds for applications that do not use bind parameters at all. Enabling these features prevents developers from intentionally using literal values.

> **See Also:**
>
> - [*Smart Logic*](where-clause-obfuscation.md#smart-logic) has more information on the execution plan caching capabilities of different databases.
> - Article: [Planning for Execution Plan Reuse](https://use-the-index-luke.com/blog/2011-07-16/planning-for-reuse)
