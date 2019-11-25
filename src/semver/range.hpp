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
public:
    enum class kind_t {
        exact,               // =
        same_minor_version,  // ~
        same_major_version,  // ^
        anything_greater,    // +
    };

    constexpr static auto exact              = kind_t::exact;
    constexpr static auto same_major_version = kind_t::same_major_version;
    constexpr static auto same_minor_version = kind_t::same_minor_version;
    constexpr static auto anything_greater   = kind_t::anything_greater;

private:
    version _base_version;
    kind_t  _kind;

public:
    range(version v, kind_t k)
        : _base_version(std::move(v))
        , _kind(k) {}

    static range parse(std::string_view str);

    std::string to_string() const noexcept;

    auto&   base_version() const noexcept { return _base_version; }
    auto    kind() const noexcept { return _kind; }
    version first_bad_version() const noexcept {
        version ret        = _base_version;
        ret.build_metadata = build_metadata();
        if (_kind == exact) {
            ret.patch++;
            return ret;
        }
        ret.patch      = 0;
        ret.prerelease = prerelease();
        if (_kind == same_minor_version) {
            ret.minor++;
            return ret;
        }
        ret.minor = 0;
        if (_kind == same_major_version) {
            ret.major++;
            return ret;
        }
        assert(false && "Cannot call first_bad_version() on a fully-open version");
        std::terminate();
    }

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
        return lhs.kind() == rhs.kind() && lhs.base_version() == rhs.base_version();
    }
};

struct range_difference {
    std::optional<range> before;
    std::optional<range> after;
};

}  // namespace semver