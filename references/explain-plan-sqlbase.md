<!-- Source: https://use-the-index-luke.com/sql/explain-plan/sqlbase — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# Working With Execution Plans in SQLBase

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/sqlbase</sub>

This appendix explains how to work with execution plans in Unify SQLBase by Gupta technologies.

Note that SQLBase seems to use an execution plan cache without any bind peeking/parameter sniffing techniques: it fully suffers from bind parameter blindness (see “[Convert bind variables to constants](http://support.guptatechnologies.com/Docs/SQLBaseDoc116/chpt18.html#830)” in the Advanced Topics Guide.

## Contents

1. *[Getting](#getting-an-execution-plan)*
2. *[Operations](#sqlbase-execution-plan-operations)*


## Getting an Execution Plan

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/sqlbase/getting-an-execution-plan</sub>

There are two ways to obtain an execution plan:

### SET PLANONLY ON

You can put the current database session into plan-only mode by issuing this command:

```
SET PLANONLY ON
```

Every sub sequentially executed command will just return it’s execution plan rather than being executed. This is the better approach to obtain an execution plan because the output is more verbose.

Of course, you can disable plan-only mode again by setting it to `OFF`.

The execution plan shown by plan-only is also stored in `SYSADM.PLAN_TABLE`. Note that you need to disable plan-only before selecting from `PLAN_TABLE`—otherwise you just plan the query on `PLAN_TABLE`.

### Session → Execution Plan / Tree-Button

After actually running a query you can obtain it’s execution plan via the menu `Session` → `Execution Plan` or by pushing the “Tree-Icon” in the toolbar. This opens a new window showing the execution plan. Note that the output format is different from the output provided by the plan-only approach.


## SQLBase Execution Plan Operations

<sub>Source: https://use-the-index-luke.com/sql/explain-plan/sqlbase/operations</sub>

A short reference of the most common SQLBase execution plan operations. That’s basically from the [API documentation](https://otdskcprd.opentext.com/otdsws/login?client_id=KC&response_mode=form_post&response_type=id_token&scope=openid%20otds%3Aroles&state=OpenIdConnect.AuthenticationProperties%3DMF3vIitnPYSRw-8z5vaNCv50Lgbjwuu8I1WnVCmV68QPybRq26aS1Zfi3J5rGawYb9fgorY8pa8ENqfafsab4vZS6ueLr0rBY_1X5GZCS7__cUvyPhe5pqLozAKRerA6chkwFNHNtweSJ-PO8-IjoAPyXvWu02z5520FCqjC1NxKTj37b5q0cGgEX9QnQZ1B&nonce=638289879473055210.NTFhZWVlYmEtZmQwNi00MzZjLWEwOTgtODM0YzM1YWVhZWY0MTlkYmY4MGItMjUzNi00MDk0LWE3M2ItOTgxZDYyZDNlZWMw&redirect_uri=https%3A%2F%2Fknowledge.opentext.com%2Fotdsagent&x-client-SKU=ID_NET461&x-client-ver=5.3.0.0#10065), which seems to be rather incomplete. The advanced guide has a second on [physical operations](http://support.guptatechnologies.com/Docs/SQLBaseDoc116/chpt16.html#481) too, but doesn’t explain how to spot them in the execution plan.

### Index and Table Access

Very much like MySQL, SQLBase shows one line per table access in the execution plan. The order of access is, again like in MySQL, top down: the first row in the execution plan corresponds to the first table accessed.

The used index will also be shown in the `INDEX` column.

SQLBase does not give indications for range vs. unique scans or index-only scans (“Index-only table access”).

The following list gives a short cross-tab of physical operations known to SQLBase to their Oracle counterpart. Unfortunately, these physical operation names are not visible in the execution plan of SQLBase:

Index leaf scan
:   Corresponds to Oracle’s `INDEX FULL SCAN`: it reads the index in order. The `Index leaf scan` also includes the subsequent tables access if necessary.

Matching index scan
:   Corresponds to Oracle’s `INDEX RANGE/UNIQUE SCAN` plus the subsequent `TABLE ACCESS BY INDEX ROWID` if necessary.

### Joins

Generally join operations process only two tables at a time. In case a query has more joins, they are executed sequentially: first two tables, then the intermediate result with the next table. In the context of joins, the term “table” could therefore also mean “intermediate result”.

SQLBase documentation as well as execution plans refer to temp tables as the intermediate result. The documentation also mentions temp tables on disk—it is unclear to me whether that means the intermediate results are always materialized.

SQLBase supports all three basic joining techniques:

NESTED LOOP / INDEX LOOP / Loop join with hash index
:   These are basically Nested Loop Joins. They just vary by the indexes being used: `Simple loop join` doesn’t use indexes at all (possibly repeating full scans on the inner table), the `Loop join with (hash) index` uses an index on the inner table (which might be a hash index in case of equijoin).

MERGE JOIN
:   The Index merge join (shown as `MERGE JOIN` in the execution plan) is a sort-merge join with the prerequisite to have indexes on the join columns for both tables—thus preventing the need for any sort operation.

HASH JOIN
:   Is a Hash Join, as the name suggests.

### Sorting and Grouping

SQLBase does not indicate sort or aggregate operations in the execution plan. It is, however, able to benefit from an index to optimize sort operations away. Note that SQLBase also supports `ASC`/`DESC` modifiers in `CREATE INDEX`.

### Top-N Queries

Top-N queries can be implemented using a session parameter:

```
SET LIMIT n
```

```
SELECT ...
```

```
SET LIMIT off
```

This doesn’t seem to affect optimization. To cause the optimizer preferring a pipelined Top-N execution when fetching a small fraction of the full result, use the [“Optimize first fetch” option](http://support.guptatechnologies.com/Docs/SQLBaseDoc116/sqltalk_cmd_ref.html#7726):

```
SET OPTIMIZEFIRSTFETCH 1
```

The optimizer will now optimize the execution plan so that the first row is returned as fast as possible. Don’t forget to revert to full optimization mode afterwards:

```
SET OPTIMIZEFIRSTFETCH 0
```

The execution plan does not indicate the presence of a Top-N limit, nor does it indicate the absence of a sort operation to indicate pipelined execution.
