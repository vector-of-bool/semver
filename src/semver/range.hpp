#pragma once

#include <semver/version.hpp>

#include <cassert>
#include <optional>
#include <stdexcept>

namespace semver {

class invalid_range : public std::runtime_error {
    std::string _str;

public:
    explicit invalid_range(std::string_view s)
        : runtime_error("Invalid version range string: " + std::string(s))
        , _str(s) {}

    auto& string() const noexcept { return _str; }
};

struct range_difference;

class range {
private:
    version _low;
    version _high;

public:
    range(version low, version high)
        : _low(std::move(low))
        , _high(std::move(high)) {
        assert(_high > _low && "Invalid range");
    }

    static range everything() noexcept { return range(version(), version::max_version()); }
    static range exactly(version v) noexcept {
        auto next = v.next_after();
        return range(std::move(v), std::move(next));
    }

    static range parse(std::string_view str);
    static range parse_restricted(std::string_view str);

    std::string to_string() const noexcept;

    auto& low() const noexcept { return _low; }
    auto& high() const noexcept { return _high; }

    std::optional<range> intersection(const range& other) const noexcept;
    std::optional<range> union_(const range& other) const noexcept;
    range_difference     difference(const range& other) const noexcept;

    bool contains(const range& other) const noexcept;
    bool contains(const version& ver) const noexcept;
    bool overlaps(const range& other) const noexcept;

    template <typename Iter, typename Stop>
    std::optional<version> max_satisfying(Iter it, const Stop stop) const noexcept {
        std::optional<version> ret;
        for (; it != stop; ++it) {
            if (!contains(*it)) {
                continue;
            }
            if (!ret.has_value()) {
                ret.emplace(*it);
            } else {
                if (*it > *ret) {
                    ret.emplace(*it);
                }
            }
        }
        return ret;
    }

    template <typename Container, typename = decltype(std::declval<Container>().cbegin())>
    std::optional<version> max_satisfying(Container&& c) const noexcept {
        return max_satisfying(c.cbegin(), c.cend());
    }

    friend bool operator==(const range& lhs, const range& rhs) noexcept {
        return lhs.low() == rhs.low() && lhs.high() == rhs.high();
    }
};

struct range_difference {
    std::optional<range> before;
    std::optional<range> after;
};

}  // namespace semver