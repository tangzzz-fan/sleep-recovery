#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <ctime>
#include <string>

// parse 12-hour US format "M/D/YYYY H:MM:SS AM/PM" → Unix timestamp
// returns -1 on parse failure
long long parse_12h_to_epoch(const std::string& s);

// Unix timestamp → ISO 8601 string "YYYY-MM-DDTHH:MM:SSZ"
std::string epoch_to_iso(long long epoch);

// format a `tm` struct → ISO 8601 string
std::string tm_to_iso(const std::tm& tm);

#endif
