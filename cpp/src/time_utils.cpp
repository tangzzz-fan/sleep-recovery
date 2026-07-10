#include "time_utils.h"
#include <sstream>
#include <iomanip>
#include <chrono>

long long parse_12h_to_epoch(const std::string& s) {
    // "M/D/YYYY H:MM:SS AM/PM" — C++ std::get_time with 12-hour format
    std::istringstream ss(s);
    std::tm tm = {};
    ss >> std::get_time(&tm, "%m/%d/%Y %I:%M:%S %p");
    if (ss.fail()) return -1;

    // timegm interprets tm as UTC (no timezone offset).
    // Python's datetime.strptime produces a naive datetime and
    // .timestamp() applies local timezone offset (PDT = UTC-8).
    // To be consistent with Python's golden sample output,
    // use timegm which directly converts to epoch treating tm as UTC.
    // This matches the data-contract.md decision: "assume all Fitbit data is UTC".
    std::time_t t = timegm(&tm);
    if (t == static_cast<std::time_t>(-1)) {
        // fallback: manual calculation
        // timegm not available on all platforms
        // Use portable offset correction:
        // Compute local offset and add it to mktime result
        std::time_t local_t = std::mktime(&tm);
        if (local_t == static_cast<std::time_t>(-1)) return -1;
        // Apply the local timezone offset (mktime → timegm)
        // On macOS/BSD, timegm IS available. This fallback is for portability.
        struct std::tm* gmt = std::gmtime(&local_t);
        if (!gmt) return -1;
        std::time_t offset = std::mktime(gmt) - local_t;
        return static_cast<long long>(local_t + offset);
    }
    return static_cast<long long>(t);
}

std::string tm_to_iso(const std::tm& tm) {
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

std::string epoch_to_iso(long long epoch) {
    if (epoch < 0) return "";
    std::time_t t = static_cast<std::time_t>(epoch);
    std::tm* tm = std::gmtime(&t);
    if (!tm) return "";
    return tm_to_iso(*tm);
}
