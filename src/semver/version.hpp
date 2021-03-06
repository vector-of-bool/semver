#pragma once

#include <semver/build_metadata.hpp>
#include <semver/prerelease.hpp>

#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace semver {

class invalid_version : public std::runtime_error {
    std::string    _string;
    std::ptrdiff_t _offset = 0;

public:
    invalid_version(std::string string, std::ptrdiff_t n)
        : runtime_error("Invalid semantic version: " + string)
        , _string(string)
        , _offset(n) {}

    auto& string() const noexcept { return _string; }
    auto  offset() const noexcept { return _offset; }
};

struct version;
order compare(const version& lhs, const version& rhs) noexcept;

struct version {
    using int_type = int;
    int_type major = 0;
    int_type minor = 0;
    int_type patch = 0;

    static constexpr int_type component_max = std::numeric_limits<int_type>::max();

    static const version& max_version() noexcept {
        static version m{component_max, component_max, component_max};
        return m;
    }

    // Prerelease tag is optional:
    class prerelease prerelease = {};
    // Build metadata is optional:
    class build_metadata build_metadata = {};

    static version parse(std::string_view s);

    std::string to_string() const noexcept;
    bool        is_prerelease() const noexcept { return !prerelease.empty(); }
    version     next_after() const noexcept;

#define DEF_OP(op, expr)                                                                           \
    inline friend bool operator op(const version& lhs, const version& rhs) noexcept {              \
        auto o = compare(lhs, rhs);                                                                \
        return (expr);                                                                             \
    }                                                                                              \
    static_assert(true)

    DEF_OP(==, (o == order::equivalent));
    DEF_OP(!=, (o != order::equivalent));
    DEF_OP(<, (o == order::less));
    DEF_OP(>, (o == order::greater));
    DEF_OP(<=, (o == order::less || o == order::equivalent));
    DEF_OP(>=, (o == order::greater || o == order::equivalent));
#undef DEF_OP

    friend inline std::string to_string(const version& ver) noexcept { return ver.to_string(); }

    friend std::ostream& operator<<(std::ostream& out, const version& ver) {
        out << ver.to_string();
        return out;
    }
};

}  // namespace semver
