<!-- Source: https://use-the-index-luke.com/sql/where-clause — "Use The Index, Luke!" by Markus Winand. Converted to Markdown for offline reference; all rights remain with the author. -->

# The Where Clause

<sub>Source: https://use-the-index-luke.com/sql/where-clause</sub>

The [previous chapter](anatomy.md) described the structure of indexes and explained the cause of poor index performance. In the next step we learn how to spot and avoid these problems in SQL statements. We start by looking at the `where` clause.

The `where` clause defines the search condition of an SQL statement, and it thus falls into the core functional domain of an index: finding data quickly. Although the `where` clause has a huge impact on performance, it is often phrased carelessly so that the database has to scan a large part of the index. The result: a poorly written `where` clause is the first ingredient of a slow query.

This chapter explains how different operators affect index usage and how to make sure that an index is usable for as many queries as possible. The last section shows common anti-patterns and presents alternatives that deliver better performance.

## Contents

1. *[The Equals Operator](where-clause-the-equals-operator.md)* — Exact key lookup

   1. *[Primary Keys](where-clause-the-equals-operator.md#primary-keys)* — Verifying index usage
   2. *[Concatenated Keys](where-clause-the-equals-operator.md#concatenated-indexes)* — Multi-column indexes
   3. *[Slow Indexes, Part II](where-clause-the-equals-operator.md#slow-indexes-part-ii)* — The first ingredient, revisited
2. *[Functions](where-clause-functions.md)* — Using functions in the `where` clause

   1. *[Case-Insensitive Search](where-clause-functions.md#case-insensitive-search-using-upper-or-lower)* — `UPPER` and `LOWER`
   2. *[User-Defined Functions](where-clause-functions.md#user-defined-functions)* — Limitations of function-based indexes
   3. *[Over-Indexing](where-clause-functions.md#over-indexing)* — Avoid redundancy
3. *[Bind Variables](where-clause-bind-parameters.md)* — For security and performance
4. *[Searching for Ranges](where-clause-searching-for-ranges.md)* — Beyond equality

   1. *[Greater, Less and `BETWEEN`](where-clause-searching-for-ranges.md#greater-less-and-between)* — The column order revisited
   2. *[Indexing SQL `LIKE` Filters](where-clause-searching-for-ranges.md#indexing-like-filters)* — `LIKE` is not for full-text search
   3. *[Index Combine](where-clause-searching-for-ranges.md#index-merge)* — Why not using one index for every column?
5. *[Partial Indexes](where-clause-partial-and-filtered-indexes.md)* — Indexing selected rows
6. *[`NULL` in the Oracle Database](where-clause-null.md)* — An important curiosity

   1. *[`NULL` in Indexes](where-clause-null.md#indexing-null)* — Every index is a partial index
   2. *[`NOT NULL` Constraints](where-clause-null.md#not-null-constraints)* — affect index usage
   3. *[Emulating Partial Indexes](where-clause-null.md#emulating-partial-indexes-in-the-oracle-database)* — using function-based indexing
7. *[Obfuscated Conditions](where-clause-obfuscation.md)* — Common anti-patterns

   1. *[Dates](where-clause-obfuscation.md#date-types)* — Pay special attention to `DATE` types
   2. *[Numeric Strings](where-clause-obfuscation.md#numeric-strings)* — Don’t mix types
   3. *[Combining Columns](where-clause-obfuscation.md#combining-columns)* — use redundant `where` clauses
   4. *[Smart Logic](where-clause-obfuscation.md#smart-logic)* — The smartest way to make SQL slow
   5. *[Math](where-clause-obfuscation.md#math)* — Databases don’t solve equations
