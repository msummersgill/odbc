#pragma once

#include <nanodbc.h>
#include <memory>
#include <Rcpp.h>
#include "r_types.h"

namespace odbconnect {
typedef std::array<const char, 255> string_buf;

class odbc_result {
  public:
    odbc_result(nanodbc::connection & c, std::string sql) :
      c_(c),
      sql_(sql) {
        s_ = nanodbc::statement(c, sql);
      };
    nanodbc::connection connection() const {
      return c_;
    }
    nanodbc::statement statement() const {
      return s_;
    }
    nanodbc::result result() const {
      return r_;
    }
    void execute() {
      if (!r_) {
        r_ = s_.execute();
      }
    }
    void insert_dataframe(Rcpp::DataFrame const & df) {
      auto types = column_types(df);
      auto ncols = df.size();
      auto nrows = df.nrows();
      size_t start = 0;
      size_t batch_size = 1024;
      nanodbc::transaction transaction(c_);

      while(start < nrows) {
        auto s = nanodbc::statement(c_, sql_);
        size_t end = start + batch_size > nrows ? nrows : start + batch_size;
        size_t size = end - start;
        clear_buffers();

        for (short col = 0; col < ncols; ++col) {
          switch(types[col]) {
            case integer_t: bind_integer(s, df, col, start, size); break;
            case double_t: bind_double(s, df, col, start, size); break;
            case string_t: bind_string(s, df, col, start, size); break;
            case datetime_t: bind_datetime(s, df, col, start, size); break;
            case date_t: bind_date(s, df, col, start, size); break;
            default: Rcpp::stop("Not yet implemented!"); break;
          }
        }
        nanodbc::execute(s, size);
        start += batch_size;
      }
      transaction.commit();
    }
    Rcpp::List fetch(int n_max = -1) {
      execute();
      return result_to_dataframe(r_, n_max);
    }

  private:
    nanodbc::connection c_;
    nanodbc::statement s_;
    nanodbc::result r_;
    std::string sql_;
    static const int seconds_in_day_ = 24 * 60 * 60;

    std::map<short, std::vector<std::string>> strings_;
    std::map<short, std::vector<nanodbc::timestamp>> times_;
    std::map<short, std::vector<uint8_t>> nulls_;

    void clear_buffers() {
      strings_.clear();
      times_.clear();
      nulls_.clear();
    }

    void bind_integer(nanodbc::statement & statement, Rcpp::DataFrame const & data, short column, size_t start, size_t size) {
      statement.bind(column, &INTEGER(data[column])[start], size, &NA_INTEGER);
    }

    // We cannot use a sentinel for doubles becuase NaN != NaN for all values
    // of NaN, even if the bits are the same.
    void bind_double(nanodbc::statement & statement, Rcpp::DataFrame const & data, short column, size_t start, size_t size) {
      nulls_[column] = std::vector<uint8_t>(size, false);

      auto vector = REAL(data[column]);
      for (size_t i = 0;i < size;++i) {
        if (ISNA(vector[start + i])) {
          nulls_[column][i] = true;
        }
      }

      statement.bind(column, &vector[start], size, reinterpret_cast<bool *>(nulls_[column].data()));
    }

    void bind_string(nanodbc::statement & statement, Rcpp::DataFrame const & data, short column, size_t start, size_t size) {
      nulls_[column] = std::vector<uint8_t>(size, false);
      for (size_t i = 0;i < size;++i) {
        auto value = STRING_ELT(data[column], start + i);
        if (value == NA_STRING) {
          nulls_[column][i] = true;
        }
        strings_[column].push_back(Rf_translateCharUTF8(value));
      }

      statement.bind_strings(column, strings_[column], reinterpret_cast<bool *>(nulls_[column].data()));
    }

    nanodbc::timestamp as_timestamp(double value) {
      nanodbc::timestamp ts;
      auto frac = modf(value, &value);
      time_t t = static_cast<time_t>(value);
      auto tm = localtime(&t);
      ts.fract = frac;
      ts.sec = tm->tm_sec;
      ts.min = tm->tm_min;
      ts.hour = tm->tm_hour;
      ts.day = tm->tm_mday;
      ts.month = tm->tm_mon + 1;
      ts.year = tm->tm_year + 1900;
      return ts;
    }

    void bind_datetime(nanodbc::statement & statement, Rcpp::DataFrame const & data, short column, size_t start, size_t size) {

      nulls_[column] = std::vector<uint8_t>(size, false);

      for (size_t i = 0;i < size;++i) {
        nanodbc::timestamp ts;
        auto value = REAL(data[column])[start + i];
        if (ISNA(value)) {
          nulls_[column][i] = true;
        } else {
          ts = as_timestamp(value);
        }
        times_[column].push_back(ts);
      }
      statement.bind(column, times_[column].data(), size, reinterpret_cast<bool *>(nulls_[column].data()));
    }
    void bind_date(nanodbc::statement & statement, Rcpp::DataFrame const & data, short column, size_t start, size_t size) {

      nulls_[column] = std::vector<uint8_t>(size, false);

      for (size_t i = 0;i < size;++i) {
        nanodbc::timestamp ts;
        auto value = REAL(data[column])[start + i] * seconds_in_day_;
        if (ISNA(value)) {
          nulls_[column][i] = true;
        } else {
          ts = as_timestamp(value);
        }
        times_[column].push_back(ts);
      }
      statement.bind(column, times_[column].data(), size, reinterpret_cast<bool *>(nulls_[column].data()));
    }

    std::vector<std::string> column_names(nanodbc::result const & r) {
      std::vector<std::string> names;
      names.reserve(r.columns());
      for (short i = 0;i < r.columns();++i) {
        names.push_back(r.column_name(i));
      }
      return names;
    }

    double as_double(nanodbc::timestamp const & ts)
    {
      tm t;
      t.tm_sec = t.tm_min = t.tm_hour = t.tm_isdst = 0;

      t.tm_year = ts.year - 1900;
      t.tm_mon = ts.month - 1;
      t.tm_mday = ts.day;
      t.tm_hour = ts.hour;
      t.tm_min = ts.min;
      t.tm_sec = ts.sec;

      return Rcpp::mktime00(t) + ts.fract;
    }

    Rcpp::List create_dataframe(std::vector<r_type> types, std::vector<std::string> names, int n) {
      int num_cols = types.size();
      Rcpp::List out(num_cols);
      out.attr("names") = names;
      out.attr("class") = "data.frame";
      out.attr("row.names") = Rcpp::IntegerVector::create(NA_INTEGER, -n);

      for (int j = 0; j < num_cols; ++j) {
        switch (types[j]) {
          case integer_t: out[j] = Rf_allocVector(INTSXP, n); break;
          case date_t:
          case datetime_t:
          case odbconnect::double_t: out[j] = Rf_allocVector(REALSXP, n); break;
          case string_t: out[j] = Rf_allocVector(STRSXP, n); break;
          case raw_t: out[j] = Rf_allocVector(VECSXP, n); break;
          case logical_t: out[j] = Rf_allocVector(LGLSXP, n); break;
        }
      }
      return out;
    }

    Rcpp::List resize_dataframe(Rcpp::List df, int n) {
      int p = df.size();

      Rcpp::List out(p);
      for (int j = 0; j < p; ++j) {
        out[j] = Rf_lengthgets(df[j], n);
      }

      out.attr("names") = df.attr("names");
      out.attr("class") = df.attr("class");
      out.attr("row.names") = Rcpp::IntegerVector::create(NA_INTEGER, -n);

      return out;
    }

    void add_classes(Rcpp::List& df, const std::vector<r_type> & types) {
      for (int col = 0; col < df.size(); ++col) {
        Rcpp::RObject x = df[col];
        switch (types[col]) {
          case date_t:
            x.attr("class") = Rcpp::CharacterVector::create("Date");
            break;
          case datetime_t:
            x.attr("class") = Rcpp::CharacterVector::create("POSIXct", "POSIXt");
            break;
          default:
            break;
        }
      }
    }

    std::vector<r_type> column_types(Rcpp::DataFrame const & df) {
      std::vector<r_type> types;
      types.reserve(df.size());
      for (short i = 0;i < df.size();++i) {
        switch(TYPEOF(df[i])) {
          case LGLSXP: types.push_back(logical_t); break;
          case INTSXP: types.push_back(integer_t); break;
          case REALSXP: {
            Rcpp::RObject x = df[i];
            if (x.inherits("Date")) {
              types.push_back(date_t);
            } else if (x.inherits("POSIXct")) {
              types.push_back(datetime_t);
            } else {
              types.push_back(double_t);
            }
            break;
          }
          case STRSXP: types.push_back(string_t); break;
          case RAWSXP: types.push_back(raw_t); break;
          default:
                       Rcpp::stop("Unsupported column type %s", Rf_type2char(TYPEOF(df[i])));
        }
      }

      return types;
    }

    std::vector<r_type> column_types(nanodbc::result const & r) {
      std::vector<r_type> types;
      types.reserve(r.columns());
      for (short i = 0;i < r.columns();++i) {

        short type = r.column_datatype(i);
        switch (type)
        {
          case SQL_BIT:
          case SQL_TINYINT:
          case SQL_SMALLINT:
          case SQL_INTEGER:
          case SQL_BIGINT:
            types.push_back(integer_t);
            break;
            // Double
          case SQL_DOUBLE:
          case SQL_FLOAT:
          case SQL_DECIMAL:
          case SQL_REAL:
          case SQL_NUMERIC:
            types.push_back(double_t);
            break;
            // Date
          case SQL_DATE:
          case SQL_TYPE_DATE:
            types.push_back(date_t);
            break;
            // Time
          case SQL_TIME:
          case SQL_TIMESTAMP:
          case SQL_TYPE_TIMESTAMP:
          case SQL_TYPE_TIME:
            types.push_back(datetime_t);
            break;
          case SQL_CHAR:
          case SQL_WCHAR:
          case SQL_VARCHAR:
          case SQL_WVARCHAR:
          case SQL_LONGVARCHAR:
          case SQL_WLONGVARCHAR:
            types.push_back(string_t);
            break;
          case SQL_BINARY:
          case SQL_VARBINARY:
          case SQL_LONGVARBINARY:
            types.push_back(raw_t);
            break;
          default:
            types.push_back(string_t);
            Rcpp::warning("Unknown field type (%s) in column %s", type, r.column_name(i));
            break;
        }
      }
      return types;
    }

    Rcpp::List result_to_dataframe(nanodbc::result & r, int n_max) {

      auto types = column_types(r);

      int n = (n_max < 0) ? 100: n_max;

      Rcpp::List out = create_dataframe(types, column_names(r), n);
      int row = 0;
      for (auto &vals : r) {
        if (row >= n) {
          if (n_max < 0) {
            n *= 2;
            out = resize_dataframe(out, n);
          } else {
            break;
          }
        }
        for (short col = 0;col < r.columns(); ++col) {
          switch(types[col]) {
            case integer_t: INTEGER(out[col])[row] = vals.get<int>(col, NA_INTEGER); break;
            case date_t:
            case datetime_t: {
              double val;

              if (vals.is_null(col)) {
                val = NA_REAL;
              } else {
                auto ts = vals.get<nanodbc::timestamp>(col);
                val = as_double(ts);
              }

              REAL(out[col])[row] = types[col] == date_t ? val / (24 * 60 * 60) : val;
              break;
            }
            case odbconnect::double_t:
                            REAL(out[col])[row] = vals.get<double>(col, NA_REAL); break;
            case string_t: {
              SEXP val;

              if (vals.is_null(col)) {
                val = NA_STRING;
              } else {
                // There is a bug/limitation in ODBC drivers for SQL Server (and possibly others)
                // which causes SQLBindCol() to never write SQL_NOT_NULL to the length/indicator
                // buffer unless you also bind the data column. nanodbc's is_null() will return
                // correct values for (n)varchar(max) columns when you ensure that SQLGetData()
                // has been called for that column (i.e. after get() or get_ref() is called).
                auto str = vals.get<std::string>(col);
                if (vals.is_null(col)) {
                  val = NA_STRING;
                } else {
                  val = Rf_mkCharCE(str.c_str(), CE_UTF8);
                }
              }
              SET_STRING_ELT(out[col], row, val); break;
            }
                            //case raw_t: out[j] = Rf_allocVector(VECSXP, n); break;
                            //case logical_t: out[j] = Rf_allocVector(LGLSXP, n); break;
            default:
                            Rcpp::warning("Unknown field type (%s) in column %s", types[col], r.column_name(col));
          }
        }

        ++row;
      }

      // Resize if needed
      if (row < n) {
        out = resize_dataframe(out, row);
      }

      add_classes(out, types);
      return out;
    }

};
}
