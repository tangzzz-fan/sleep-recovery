#include "csv_reader.h"
#include <cassert>

CsvReader::CsvReader(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) {
        m_error = "cannot open file: " + filepath;
        return;
    }

    std::string line;

    // read header
    if (!std::getline(f, line)) {
        m_error = "empty file: " + filepath;
        return;
    }
    // trim trailing \r (Windows line endings)
    if (!line.empty() && line.back() == '\r') line.pop_back();
    m_columns = split_line(line);
    if (m_columns.empty()) {
        m_error = "no columns in header";
        return;
    }

    // read data rows
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        // trim trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto fields = split_line(line);
        if (fields.size() != m_columns.size()) {
            // skip malformed rows (missing fields)
            continue;
        }
        CsvRow row;
        for (size_t i = 0; i < m_columns.size(); ++i) {
            row[m_columns[i]] = fields[i];
        }
        m_rows.push_back(std::move(row));
    }

    m_ok = true;
}

std::vector<std::string> CsvReader::split_line(const std::string& line) {
    std::vector<std::string> result;
    std::string field;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            if (in_quotes && i + 1 < line.size() && line[i + 1] == '"') {
                // escaped quote: ""
                field += '"';
                ++i;
            } else {
                in_quotes = !in_quotes;
            }
        } else if (c == ',' && !in_quotes) {
            result.push_back(field);
            field.clear();
        } else {
            field += c;
        }
    }
    result.push_back(field);
    return result;
}
