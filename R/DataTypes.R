#' Return the corresponding ODBC data type for an R object
#'
#' This is used when creating a new table with `dbWriteTable()`.
#' Databases with default methods defined are
#' - MySQL
#' - PostgreSQL
#' - SQL Server
#' - SQLite
#' - Spark
#' - Hive
#' - Impala
#' - Redshift
#' - Vertica
#'
#' If you are using a different database and `dbWriteTable()` fails with a SQL
#' parsing error the default method is not appropriate, you will need to write
#' a new method.
#'
#' @section Defining a new dbDataType method:
#'
#' The object type for your connection will be the database name retrieved by
#' `dbGetInfo(con)$dbms.name`. Use the documentation provided with your
#' database to determine appropriate values for each R data type. An example
#' method definition of a fictional `foo` database follows.
#' ```
#' con <- dbConnect(odbc::odbc(), "FooConnection")
#' dbGetInfo(con)$dbms.name
#' #> [1] "foo"
#'
#' `odbcDataType.foo <- function(con, obj, ...) {
#'   switch_type(obj,
#'     factor = "VARCHAR(255)",
#'     datetime = "TIMESTAMP",
#'     date = "DATE",
#'     binary = "BINARY",
#'     integer = "INTEGER",
#'     double = "DOUBLE",
#'     character = "VARCHAR(255)",
#'     logical = "BIT",
#'     list = "VARCHAR(255)",
#'     stop("Unsupported type", call. = FALSE)
#'   )
#' }
#' ```
#' @param con A driver connection object, as returned by `dbConnect()`.
#' @param obj An R object.
#' @param ... Additional arguments passed to methods.
#' @return Corresponding SQL type for the `obj`.
#' @export
odbcDataType <- function(con, obj, ...) UseMethod("odbcDataType")

#' @export
odbcDataType.default <- function(con, obj, ...) {
  switch_type(obj,
    factor = "VARCHAR(255)",
    datetime = "TIMESTAMP",
    date = "DATE",
    time = "TIME",
    binary = "VARBINARY(255)",
    integer = "INTEGER",
    double = "DOUBLE PRECISION",
    character = "VARCHAR(255)",
    logical = "BIT", # only valid if DB supports Null fields
    list = "VARCHAR(255)",
    stop("Unsupported type", call. = FALSE)
  )
}


#' @export
`odbcDataType.Redshift` <- function(con, obj, ...) {
  switch_type(obj,
    factor = "VARCHAR(255)",
    datetime = "TIMESTAMP",
    date = "DATE",
    integer = "INTEGER",
    double = "DOUBLE PRECISION",
    character = "VARCHAR(255)",
    logical = "BOOLEAN",
    list = "VARCHAR(255)",
    time = ,
    binary =,
    stop("Unsupported type", call. = FALSE)
  )
}

#' @export
`odbcDataType.Spark SQL` <- function(con, obj, ...) {
  switch_type(obj,
    factor = "VARCHAR(255)",
    datetime = "DATE",
    date = "DATE",
    binary = "BINARY",
    integer = "INT",
    double = "DOUBLE",
    character = "VARCHAR(255)",
    logical = "BOOLEAN",
    list = "VARCHAR(255)",
    time = ,
    stop("Unsupported type", call. = FALSE)
  )
}

#' @export
`odbcDataType.Hive` <- function(con, obj, ...) {
  switch_type(obj,
    factor = "STRING",
    datetime = "TIMESTAMP",
    date = "DATE",
    binary = "BINARY",
    integer = "INT",
    double = "DOUBLE",
    character = "STRING",
    logical = "BOOLEAN",
    list = "STRING",
    time = ,
    stop("Unsupported type", call. = FALSE)
  )
}

# TODO: Revisit binary type (Impala)
#' @export
`odbcDataType.Impala` <- function(con, obj, ...) {
  switch_type(obj,
    factor = "STRING",
    datetime = "STRING",
    date = "VARCHAR(10)",
    integer = "INT",
    double = "DOUBLE",
    character = "STRING",
    logical = "BOOLEAN",
    list = "STRING",
    time = ,
    stop("Unsupported type", call. = FALSE)
    )
}

#' @export
`odbcDataType.MySQL` <- function(con, obj, ...) {
  switch_type(obj,
    factor = "TEXT",
    datetime = "DATETIME",
    date = "DATE",
    time = "TIME",
    binary = "BLOB",
    integer = "INTEGER",
    double = "DOUBLE",
    character = "TEXT",
    logical = "TINYINT",
    list = "TEXT",
    stop("Unsupported type", call. = FALSE)
  )
}

#' @export
`odbcDataType.PostgreSQL` <- function(con, obj, ...) {
  switch_type(obj,
    factor = "TEXT",
    datetime = "TIMESTAMP",
    date = "DATE",
    time = "TIME",
    binary = "bytea",
    integer = "INTEGER",
    double = "DOUBLE PRECISION",
    character = "TEXT",
    logical = "BOOLEAN",
    list = "TEXT",
    stop("Unsupported type", call. = FALSE)
  )
}

#' @export
`odbcDataType.Vertica Database` <- function(con, obj, ...) {
  switch_type(obj,
    factor = "VARCHAR",
    datetime = "TIMESTAMP",
    date = "DATE",
    integer = "INTEGER",
    double = "DOUBLE PRECISION",
    character = "VARCHAR",
    logical = "BOOLEAN",
    list = "VARCHAR",
    time = "TIME",
    binary = "VARBINARY",
    stop("Unsupported type", call. = FALSE)
  )
}

#' @export
`odbcDataType.Microsoft SQL Server` <- function(con, obj, ...) {
  switch_type(obj,
    factor = varchar(obj),
    datetime = "DATETIME",
    date = "DATE",
    time = "TIME",
    binary = varbinary(obj),
    integer = "INT",
    double = "FLOAT",
    character = varchar(obj),
    logical = "BIT",
    list = varchar(obj),
    stop("Unsupported type", call. = FALSE)
  )
}

#' @export
odbcDataType.Oracle <- function(con, obj, ...) {
  switch_type(obj,
     factor = "VARCHAR(255)",

     # No native oracle type for time
     time = "VARCHAR(255)",

     # There is a native type for date, but the default input format may not be
     # ISO 8601, it is determined by NLS_DATE_FORMAT or derived from
     # NLS_TERRITORY, so use character as a fallback.
     date = "VARCHAR(255)",

     # datetime errors with * character string is not in a standard unambiguous
     # format, even with character input.
#    datetime = "VARCHAR(255)",
     binary = "BLOB",
     integer = "INTEGER",
     double = "BINARY_DOUBLE",
     character = "VARCHAR(255)",
     logical = "DECIMAL",
     list = "VARCHAR(255)",
     stop("Unsupported type", call. = FALSE)
  )
}

#' @export
`odbcDataType.SQLite` <- function(con, obj, ...) {
  switch_type(obj,
    factor = "TEXT",
    datetime = "NUMERIC",
    date = "NUMERIC",
    binary = "BLOB",
    integer = "INTEGER",
    double = "REAL",
    character = "TEXT",
    logical = "NUMERIC",
    list = "TEXT",
    stop("Unsupported type", call. = FALSE)
  )
}

switch_type <- function(obj, ...) {
  switch(object_type(obj), ...)
}

object_type <- function(obj) {
  if (is.factor(obj)) return("factor")
  if (is(obj, "POSIXct")) return("datetime")
  if (is(obj, "Date")) return("date")
  if (is(obj, "blob")) return("binary")
  if (is(obj, "difftime")) return("time")

  return(typeof(obj))
}

varchar <- function(x, type = "varchar") {
  max_length <- max(nchar(as.character(x)), na.rm = TRUE)
  paste0(type, "(", max(255, max_length), ")")
}

varbinary <- function(x, type = "varbinary") {
  max_length <- max(lengths(x), na.rm = TRUE)
  paste0(type, "(", max(255, max_length), ")")
}

