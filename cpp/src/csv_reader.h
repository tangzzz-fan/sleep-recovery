#ifndef CSV_READER_H
#define CSV_READER_H

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>

// tiny CSV reading without external deps
// represents a single row: column_name → value
using CsvRow = std::map<std::string, std::string>;
using CsvTable = std::vector<CsvRow>;

class CsvReader {
public:
    explicit CsvReader(const std::string& filepath);

    // returns all rows
    const CsvTable& rows() const { return m_rows; }

    // column names from header
    const std::vector<std::string>& columns() const { return m_columns; }

    // number of data rows (excluding header)
    size_t size() const { return m_rows.size(); }

    bool ok() const { return m_ok; }
    std::string error() const { return m_error; }

private:
    std::vector<std::string> m_columns;
    CsvTable m_rows;
    bool m_ok = false;
    std::string m_error;

    std::vector<std::string> split_line(const std::string& line);
};

#endif
