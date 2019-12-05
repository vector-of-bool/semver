#include "./range.hpp"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <tuple>

using namespace semver;

range range::parse(const std::string_view str) {
    auto lt_pos = str.find('<');
    if (lt_pos == str.npos) {
        return parse_restricted(str);
    }
    auto low_str  = str.substr(0, lt_pos);
    auto high_str = str.substr(lt_pos + 1);
    auto low      = version::parse(low_str);
    auto high     = version::parse(high_str);
    if (high <= low) {
        throw invalid_range(str);
    }
    return {std::move(low), std::move(high)};
}

range range::parse_restricted(const std::string_view str) {
    if (str == "*") {
        return {version(), version::max_version()};
    }
    if (str.empty()) {
        throw invalid_range(str);
    }

    auto       ptr  = str.data();
    const auto stop = ptr + str.size();
    if (*ptr == '=' || std::isdigit(*ptr)) {
        if (*ptr == '=') {
            ++ptr;
        }
        auto ver = version::parse(std::string_view(ptr, stop - ptr));
        return {ver, ver.next_after()};
    } else if (*ptr == '~') {
        ++ptr;
        auto ver = version::parse(std::string_view(ptr, stop - ptr));
        auto v2  = ver;
        v2.patch = version::component_max;
        return {ver, v2.next_after()};
    } else if (*ptr == '^') {
        ++ptr;
        auto ver = version::parse(std::string_view(ptr, stop - ptr));
        auto v2  = ver;
        v2.patch = version::component_max;
        v2.minor = version::component_max;
        return {ver, v2.next_after()};
    } else if (*ptr == '+') {
        ++ptr;
        auto ver = version::parse(std::string_view(ptr, stop - ptr));
        return {ver, version::max_version()};
    } else {
        throw invalid_range(str);
    }
}

std::string range::to_string() const noexcept {
    std::string ret;
    if (low() == high()) {
        return low().to_string();
    }
    if (high() == version::max_version()) {
        return low().to_string() + "+";
    }
    return low().to_string() + "<" + high().to_string();
}

bool range::contains(const version& other) const noexcept {
    return (low() <= other) && (high() > other);
}

std::optional<range> range::intersection(const range& other) const noexcept {
    const auto& low_  = (std::max)(low(), other.low());
    const auto& high_ = (std::min)(high(), other.high());
    if (low_ < high_) {
        return range{low_, high_};
    } else {
        return std::nullopt;
    }
}

std::optional<range> range::union_(const range& other) const noexcept {
    const auto& low_  = (std::min)(low(), other.low());
    const auto& high_ = (std::max(high(), other.high()));
    return range{low_, high_};
}

namespace {

std::optional<range> diff_before(const range& self, const range& other) noexcept {
    if (self.low() >= other.low()) {
        // Nothing below
        return std::nullopt;
    } else {
        return range{self.low(), other.low()};
    }
}

std::optional<range> diff_after(const range& self, const range& other) noexcept {
    if (self.high() <= other.high()) {
        return std::nullopt;
    } else {
        return range{other.high(), self.high()};
    }
}

}  // namespace

range_difference range::difference(const range& other) const noexcept {
    if (!this->overlaps(other)) {
        // No overlap. Nothing to remove
        if (low() < other.low()) {
            return {*this, std::nullopt};
        } else {
            assert(low() > other.low());
            return {std::nullopt, *this};
        }
    }
    return {
        diff_before(*this, other),
        diff_after(*this, other),
    };
}

bool range::contains(const range& other) const noexcept {
    return (low() <= other.low()) && (high() >= other.high());
}

bool range::overlaps(const range& other) const noexcept {
    return contains(other.low()) || other.contains(low());
}
