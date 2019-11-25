#include "./range.hpp"

#include <cassert>
#include <charconv>
#include <tuple>

using namespace semver;

range range::parse(const std::string_view str) {
    if (str == "*") {
        return {version(), anything_greater};
    }
    if (str.empty()) {
        throw invalid_range(str);
    }

    auto       ptr  = str.data();
    const auto stop = ptr + str.size();
    kind_t     mode = anything_greater;
    if (*ptr == '=') {
        mode = range::exact;
        ++ptr;
    } else if (*ptr == '~') {
        mode = same_minor_version;
        ++ptr;
    } else if (*ptr == '^') {
        mode = same_major_version;
        ++ptr;
    } else if (*ptr == '+') {
        mode = anything_greater;
        ++ptr;
    } else if (std::isdigit(*ptr)) {
        mode = range::exact;
    } else {
        throw invalid_range(str);
    }

    auto bv = version::parse(std::string_view(ptr, stop - ptr));
    return {std::move(bv), mode};
}

std::string range::to_string() const noexcept {
    std::string ret;
    if (_kind == exact) {
        ret.push_back('=');
    } else if (_kind == same_minor_version) {
        ret.push_back('~');
    } else if (_kind == same_major_version) {
        ret.push_back('^');
    } else if (_kind == anything_greater) {
        ret.push_back('+');
    } else {
        assert(false && "Unreachable");
        std::terminate();
    }
    ret += _base_version.to_string();
    return ret;
}

bool range::contains(const version& ver) const noexcept {
    if (ver.is_prerelease() != base_version().is_prerelease()) {
        // Prerelease versions shouldnt' be considered unless the spec basis
        // version is also a prerelease
        return false;
    }
    // Anything greater than the basis version is satisfactory
    if (_kind == anything_greater) {
        return ver >= base_version();
    }
    // At least the major component must match
    if (_kind == same_major_version) {
        // We only need the major version to match
        if (ver.major != base_version().major) {
            return false;
        }
        // Minor, patch, and prerelease must be >=
        auto tie = [](auto&& vsn) { return std::tie(vsn.minor, vsn.patch, vsn.prerelease); };
        return tie(ver) >= tie(base_version());
    }
    if (_kind == same_minor_version) {
        // We need major _and_ minor to match
        if (std::tie(ver.major, ver.minor)
            != std::tie(base_version().major, base_version().minor)) {
            return false;
        }
        // Patch and prerelease must be >=
        auto tie = [](auto&& vsn) { return std::tie(vsn.patch, vsn.prerelease); };
        return tie(ver) >= tie(base_version());
    }
    if (_kind == range::exact) {
        // All components must match exactly
        return ver == base_version();
    }
    std::terminate();
}

std::optional<range> range::intersection(const range& other) const noexcept {
    // First, ensure that *this has the lower basis version.
    if (other._base_version < _base_version) {
        // Intersection is commutative
        return other.intersection(*this);
    }

    auto& other_base_verison = other.base_version();
    auto& this_base_version  = base_version();
    auto  other_kind         = other.kind();
    auto  this_kind          = kind();

    // Known: verison <= other.version
    if (kind() == anything_greater) {
        // We extend infinitely, and `other` is >= `this`, so:
        return other;
    }
    if (other_base_verison.major > this_base_version.major) {
        // Too new
        return std::nullopt;
    }

    // Known: major == other.major
    if (this_kind == same_major_version) {
        if (other_kind == anything_greater) {
            return range(other_base_verison, same_major_version);
        }
        return other;
    }
    if (other_base_verison.minor > this_base_version.minor) {
        // Too new
        return std::nullopt;
    }

    // Known: minor == other.minor
    if (this_kind == same_minor_version) {
        if (other_kind == anything_greater || other_kind == same_major_version) {
            return range(other_base_verison, same_minor_version);
        }
        return other;
    }
    if (other_base_verison.patch > this_base_version.patch) {
        // Too new
        return std::nullopt;
    }

    // Known: patch == other.patch
    if (this_kind == exact) {
        return *this;
    }

    assert(false && "Unreachable");
    std::terminate();
}

std::optional<range> range::union_(const range& other) const noexcept {
    // Ensure that we have the lower base version
    if (other.base_version() < base_version()) {
        // Union is commutative
        return other.union_(*this);
    }

    auto& other_version = other.base_version();
    auto& this_verison  = base_version();
    auto  other_kind    = other.kind();
    auto  this_kind     = kind();

    // Known: version <= other.version
    if (this_kind == anything_greater) {
        // We already subsume `other`
        return *this;
    }

    auto this_first_bad_version = first_bad_version();

    if (other_version > this_first_bad_version) {
        // `other` is too high. No contiguous union can be formed.
        return std::nullopt;
    }
    if (other_version == this_first_bad_version) {
        // (literal) edge case. The bottom of `other` is at the top of `this`.
        // We may form a union as long as `other` is more lenient. Otherwise,
        // we cannot represent such a union.
        if (other_kind == anything_greater) {
            // Okay: `other` starts at the end and is infinite
            return range(this_verison, anything_greater);
        } else if (other_kind == same_major_version && this_kind != same_major_version) {
            assert(this_kind != anything_greater);
            return range(this_verison, same_major_version);
        } else if (other_kind == same_minor_version && this_kind == exact) {
            return range(this_verison, same_minor_version);
        }
        // Any other union is not representable
        /**
         * Maybe you got here and you think. "Hey no! I _can_ form a contiguous range for X and Y!"
         * If you are thinking of forming a union of ^1.0.0 with ^2.0.0, to represent
         * "everything 1.x.x and everythin 2.x.x," know that such a version range is not only
         * _unrepresentable_ in semver::range, but that such a version range is semantically
         * problematic. The incrementing of a component of a version numbers either breaking or not:
         * We purposefully do not support a concept of "Major versions [N, N+X) are API compatible,
         * but not [N+X, N+X+1)."
         *
         * The use of `>=` above is purposeful!
         */
        return std::nullopt;
    }

    if (other_kind == anything_greater) {
        // `other` starts within our range and continues infinitely
        return range(this_verison, anything_greater);
    }

    auto other_first_bad_version = other.first_bad_version();
    if (this_first_bad_version >= other_first_bad_version) {
        // We already contain all of `other`
        return *this;
    }

    // The bottom of `other` lands within our range, but the top lives beyond.
    // The union is the lower version (which is our version) and their more leniant
    // version constraint
    return range(this_verison, other_kind);
}

bool range::contains(const range& other) const noexcept {
    auto  other_kind    = other._kind;
    auto& other_version = other.base_version();
    auto& this_version  = base_version();

    if (other_version < this_version) {
        // Other version cannot possibly be a subset.
        return false;
    }

    // Known: other_verison >= this_version
    if (_kind == anything_greater) {
        return true;
    }
    if (other_kind == anything_greater) {
        return false;
    }

    // Both finite ranges. Our bottom is below the other. We completely enclose
    // the other if our top is at least as great as the other's
    return first_bad_version() >= other.first_bad_version();
}

bool range::overlaps(const range& other) const noexcept {
    return contains(other.base_version()) || other.contains(_base_version);
}
