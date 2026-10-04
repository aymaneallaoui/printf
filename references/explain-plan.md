<!-- Source: https://use-the-index-luke.com/sql/explain-plan — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# Execution Plans

<sub>Source: https://use-the-index-luke.com/sql/explain-plan</sub>

Before the database can execute an SQL statement, the optimizer has to create an execution plan for it. The database then executes this plan in a step-by-step manner. In this respect, the optimizer is very similar to a compiler because it translates the source code (SQL statement) into an executable program (execution plan).

The execution plan is the first place to look when searching for the cause of slow statements. The following sections explain how to retrieve and read an execution plan to optimize performance in various databases.

## Contents

1. *[Db2 (LUW)](explain-plan-db2.md)* : *[Getting](explain-plan-db2.md#getting-an-execution-plan)* • *[Operations](explain-plan-db2.md#db2-luw-execution-plan-operations)* • *[Access vs. filter predicates](explain-plan-db2.md#distinguishing-access-and-filter-predicates)*
2. *[MySQL](explain-plan-mysql.md)* : *[Getting](explain-plan-mysql.md#getting-an-execution-plan)* • *[Operations](explain-plan-mysql.md#operations)* • *[Access vs. filter predicates](explain-plan-mysql.md#distinguishing-access-and-filter-predicates)*
3. *[Oracle](explain-plan-oracle.md)* : *[Getting](explain-plan-oracle.md#getting-an-execution-plan)* • *[Operations](explain-plan-oracle.md#operations)* • *[Access vs. filter predicates](explain-plan-oracle.md#distinguishing-access-and-filter-predicates)*
4. *[PostgreSQL](explain-plan-postgresql.md)* : *[Getting](explain-plan-postgresql.md#getting-an-execution-plan)* • *[Operations](explain-plan-postgresql.md#operations)* • *[Access vs. filter predicates](explain-plan-postgresql.md#distinguishing-access-and-filter-predicates)*
5. *[SQL Server](explain-plan-sql-server.md)* : *[Getting](explain-plan-sql-server.md#getting-an-execution-plan)* • *[Operations](explain-plan-sql-server.md#operations)* • *[Access vs. filter predicates](explain-plan-sql-server.md#distinguishing-access-and-filter-predicates)*
6. *[SQLite](explain-plan-sqlite.md)* : *[Getting](explain-plan-sqlite.md#getting-an-execution-plan)* • *[Operations](explain-plan-sqlite.md#sqlite-execution-plan-operations)*
7. *[Gupta SQLBase](explain-plan-sqlbase.md)* : *[Getting](explain-plan-sqlbase.md#getting-an-execution-plan)* • *[Operations](explain-plan-sqlbase.md#sqlbase-execution-plan-operations)*
