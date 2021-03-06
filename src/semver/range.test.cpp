#include <semver/range.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Parse a version spec string") { auto spec = semver::range::parse("1.2.3"); }

TEST_CASE("Parse range specifiers") {
    struct case_ {
        std::string_view spec;
        std::string_view low;
        std::string_view high;
    };
    auto [spec, low_str, high_str] = GENERATE(Catch::Generators::values<case_>({
        {"1.2.3", "1.2.3", "1.2.4"},
        {"1.2.3<5.6.7", "1.2.3", "5.6.7"},
        {"~1.2.3", "1.2.3", "1.3.0"},
        {"^1.2.3", "1.2.3", "2.0.0"},
    }));
    INFO("Checking range string: " << spec);
    auto low   = semver::version::parse(low_str);
    auto high  = semver::version::parse(high_str);
    auto range = semver::range::parse(spec);
    CHECK(range.low() == low);
    CHECK(range.high() == high);
}

TEST_CASE("Parse an asterisk") {
    auto spec = semver::range::parse("*");
    CHECK(spec.low() == semver::version{0, 0, 0});
}

TEST_CASE("Check version compatibility") {
    struct case_ {
        std::string_view version;
        std::string_view spec;
        bool             expect_compatible;
    };
    auto [version_str, range_str, expect_satisfied] = GENERATE(Catch::Generators::values<case_>({
        {"1.2.3", "*", true},
        {"1.2.3", "1.2.3", true},
        {"1.2.3", "=1.2.3", true},
        {"1.2.3", "+1.2.3", true},
        {"1.2.3", "+1.2.4", false},
        {"1.2.3-alpha", "+1.2.3", false},
        {"1.2.3-alpha", "+1.2.0", true},
        {"1.2.3", "^1.2.3", true},
        {"1.3.3", "^1.2.3", true},
        {"1.0.3", "^1.2.3", false},
        {"2.2.3", "^1.2.3", false},
        {"1.0.0", "^1.2.0", false},
        {"2.0.0", "+1.2.0", true},
        {"1.2.0", "~1.2.0", true},
        {"1.2.1", "~1.2.0", true},
        {"1.2.0", "~1.2.1", false},
        {"1.3.0", "~1.2.99", false},
    }));
    INFO("Should " << version_str << " satisfy the spec " << range_str << " ? "
                   << expect_satisfied);
    auto ver    = semver::version::parse(version_str);
    auto spec   = semver::range::parse(range_str);
    auto actual = spec.contains(ver);
    CHECK(actual == expect_satisfied);
}

struct version_seq {
    std::vector<semver::version> versions;

    template <typename... Strings>
    version_seq(Strings... s)
        : versions({semver::version::parse(s)...}) {}
};

TEST_CASE("Find the max satisfying version") {
    struct case_ {
        std::string_view                spec;
        std::optional<std::string_view> expect_version;
        version_seq                     versions;
    };
    auto [spec_str, expect, versions] = GENERATE(Catch::Generators::values<case_>({
        {"+1.2.3", "1.2.4", {"1.2.3", "1.2.4"}},
        {"^1.2.3", "1.8.3", {"1.0.3", "3.1.2", "1.8.3"}},
        {"~1.2.3", "1.2.6", {"1.0.3", "3.1.2", "1.2.6", "1.8.3"}},
        {"=1.2.3", "1.2.3", {"1.2.3", "1.2.4"}},
        {"1.2.3", "1.2.3", {"1.2.3", "1.2.4"}},
    }));
    INFO("Checking range " << spec_str << " to find match " << expect.value_or("[nothing]"));
    auto spec = semver::range::parse(spec_str);
    auto sat  = spec.max_satisfying(versions.versions);
    if (expect) {
        INFO("max_satisfying() returned " << sat->to_string());
        CHECK(sat == semver::version::parse(*expect));
    } else {
        CHECK(!sat.has_value());
    }
}

TEST_CASE("Finding the first invalid version") {
    struct case_ {
        std::string_view range;
        std::string_view expected;
    };
    auto [rng_str, expect] = GENERATE(Catch::Generators::values<case_>({
        {"1.0.0", "1.0.1"},
        {"~1.0.0", "1.1.0"},
        {"^1.0.0", "2.0.0"},
        {"1.2.3", "1.2.4"},
        {"~1.2.3", "1.3.0"},
        {"^1.2.3", "2.0.0"},
    }));
    INFO("Checking range: " << rng_str);
    auto rng              = semver::range::parse(rng_str);
    auto next_bad_version = rng.high();
    CHECK(next_bad_version == semver::version::parse(expect));
}

TEST_CASE("Range intersection") {
    struct case_ {
        std::string_view                a;
        std::string_view                b;
        std::optional<std::string_view> expected;
    };
    auto [a_str, b_str, expect] = GENERATE(Catch::Generators::values<case_>({
        {"1.0.0", "1.0.0", "=1.0.0"},
        {"1.0.0", "+1.0.0", "=1.0.0"},
        {"^1.2.3", "^1.2.0", "^1.2.3"},
        {"^1.2.3", "~1.3.0", "~1.3.0"},
        {"^1.2.3", "~1.1.0", std::nullopt},
        {"+0.1.2", "~7.4.3", "~7.4.3"},
        {"^1.7.2", "+1.9.2", "^1.9.2"},
        {"~1.7.2", "^1.7.9", "~1.7.9"},
        {"1.2.3", "2.3.4", std::nullopt},
        {"^1.2.3", "2.0.0", std::nullopt},
    }));
    INFO("Checking the intersection of " << a_str << " with " << b_str);
    INFO("Expected intersection: " << expect.value_or("[nothing]"));
    auto a            = semver::range::parse(a_str);
    auto b            = semver::range::parse(b_str);
    auto intersection = a.intersection(b);
    if (intersection) {
        INFO("Actual intersection:   " << intersection->to_string());
        REQUIRE(expect.has_value());
        CHECK(intersection == semver::range::parse(*expect));
        // Check for commutativity
        CHECK(intersection == b.intersection(a));
    } else {
        INFO("Actual intersection:   [null]");
        CHECK_FALSE(expect.has_value());
        CHECK_FALSE(intersection.has_value());
        // Check for commutativity
        CHECK(intersection == b.intersection(a));
    }
}

TEST_CASE("Range union") {
    struct case_ {
        std::string_view                a;
        std::string_view                b;
        std::optional<std::string_view> expected;
    };
    auto [a_str, b_str, expect] = GENERATE(Catch::Generators::values<case_>({
        {"1.0.0", "1.0.0", "=1.0.0"},
        {"1.0.0", "+1.0.0", "+1.0.0"},
        {"1.2.0", "+1.0.0", "+1.0.0"},
        {"^1.2.0", "+1.0.0", "+1.0.0"},
        {"~1.2.0", "+1.0.0", "+1.0.0"},
        {"~1.2.0", "+1.0.0", "+1.0.0"},
        {"~1.2.0", "1.2.3", "~1.2.0"},
        {"^1.2.0", "~1.2.3", "^1.2.0"},
        {"~1.2.0", "^1.1.9", "^1.1.9"},
        {"~1.2.0", "^1.1.9", "^1.1.9"},
        {"^1.6.2", "4.1.2", "1.6.2<4.1.3"},
        {"^1.6.2", "~2.0.0", "1.6.2<2.1.0"},
        {"+1.2.0", "~1.1.3", "+1.1.3"},
        {"^1.2.0", "~1.1.0", "^1.1.0"},
        {"~1.2.4", "=1.2.4", "~1.2.4"},
    }));
    INFO("Checking the union of " << a_str << " with " << b_str);
    INFO("Expected union: " << expect.value_or("[nothing]"));
    auto a      = semver::range::parse(a_str);
    auto b      = semver::range::parse(b_str);
    auto union_ = a.union_(b);
    if (union_) {
        INFO("Actual union:   " << union_->to_string());
        REQUIRE(expect.has_value());
        CHECK(union_ == semver::range::parse(*expect));
        // Check for commutativity
        CHECK(union_ == b.union_(a));
    } else {
        INFO("Actual union:   [null]");
        CHECK_FALSE(expect.has_value());
        CHECK_FALSE(union_.has_value());
        // Check for commutativity
        CHECK(union_ == b.union_(a));
    }
}

TEST_CASE("Range difference") {
    struct case_ {
        std::string_view                left;
        std::string_view                right;
        std::optional<std::string_view> exp_before;
        std::optional<std::string_view> exp_after;
    };

    auto [left_str, right_str, exp_before, exp_after] = GENERATE(Catch::Generators::values<case_>({
        {"1.2.3", "1.2.4", "1.2.3", std::nullopt},
        {"^1.2.3", "1.4.6", "1.2.3<1.4.6", "1.4.7<2.0.0"},
        {"^1.0.0", "~1.6.0", "1.0.0<1.6.0", "1.7.0<2.0.0"},
        {"^1.2.3", "^2.3.4", "^1.2.3", std::nullopt},
        {"^1.2.3", "^0.3.4", std::nullopt, "^1.2.3"},
        {"~1.2.4", "^1.1.4", std::nullopt, std::nullopt},
        {"~1.2.4", "^1.2.6", "1.2.4<1.2.6", std::nullopt},
        {"+1.2.3", "^3.0.0", "1.2.3<3.0.0", "+4.0.0"},
        {"^4.3.8", "+4.6.4", "4.3.8<4.6.4", std::nullopt},
    }));
    INFO("Check difference of '" << left_str << "' and '" << right_str << "'");
    auto left            = semver::range::parse(left_str);
    auto right           = semver::range::parse(right_str);
    auto [before, after] = left.difference(right);
    CHECK(before.has_value() == exp_before.has_value());
    if (before) {
        INFO("before   = " << before->to_string());
        REQUIRE(exp_before);
        INFO("expected = " << *exp_before);
        CHECK(*before == semver::range::parse(*exp_before));
    }
    CHECK(after.has_value() == exp_after.has_value());
    if (after) {
        INFO("after    = " << after->to_string());
        REQUIRE(exp_after);
        INFO("expected = " << *exp_after);
        CHECK(*after == semver::range::parse(*exp_after));
    }
}

TEST_CASE("Inclusion") {
    struct case_ {
        std::string_view outer;
        std::string_view inner;
        bool             includes;
    };
    auto [outer, inner, expect] = GENERATE(Catch::Generators::values<case_>({
        /// /////////////////////////////////////////
        /// Same basis version
        {"1.2.3", "1.2.3", true},
        {"~1.2.3", "1.2.3", true},
        {"^1.2.3", "1.2.3", true},
        {"+1.2.3", "1.2.3", true},
        /// /////////////////////////////////////////
        /// Older basis version can never match
        {"1.2.3", "1.2.2", false},
        {"~1.2.3", "1.2.1", false},
        {"^1.2.3", "1.2.2", false},
        {"+1.2.3", "1.2.1", false},
        /// /////////////////////////////////////////
        /// Newer major version
        {"1.2.3", "2.2.3", false},
        {"~1.2.3", "2.2.3", false},
        {"^1.2.3", "2.2.3", false},
        {"+1.2.3", "2.2.3", true},
        /// /////////////////////////////////////////
        /// Newer minor version
        {"1.2.3", "1.3.3", false},
        {"~1.2.3", "1.3.3", false},
        {"^1.2.3", "1.3.3", true},
        {"+1.2.3", "1.3.3", true},
        /// /////////////////////////////////////////
        /// Newer patch version
        {"1.2.3", "1.2.4", false},
        {"~1.2.3", "1.2.4", true},
        {"^1.2.3", "1.2.4", true},
        {"+1.2.3", "1.2.4", true},
        /// /////////////////////////////////////////
        /// Large ranges don't fit
        {"1.2.3", "~1.2.3", false},
        {"~1.2.3", "^1.2.3", false},
        {"^1.2.3", "+1.2.3", false},
        /// /////////////////////////////////////////
        /// Smaller subranges _might_ fit
        {"1.2.3", "~1.2.4", false},
        {"~1.2.3", "~1.2.7", true},
        {"^1.2.3", "~1.4.4", true},
        {"^1.2.3", "^1.4.4", true},
        {"+1.2.3", "^1.2.4", true},
        {"+1.2.3", "^1.2.0", false},
        {"+1.2.3", "~2.2.1", true},
    }));
    INFO("Check whether range '" << outer << "' contains '" << inner << "'");
    CHECK(semver::range::parse(outer).contains(semver::range::parse(inner)) == expect);
}

TEST_CASE("Version overlap") {
    struct case_ {
        std::string_view lhs;
        std::string_view rhs;
        bool             should_overlap;
    };
    auto [lhs, rhs, should_overlap] = GENERATE(Catch::Generators::values<case_>({
        {"1.2.3", "1.2.3", true},
        {"1.2.3", "^1.2.3", true},
        {"1.1.3", "^1.2.3", false},
        {"1.5.3", "^1.2.3", true},
        {"~1.5.3", "^1.2.3", true},
        {"+1.5.3", "^1.2.3", true},
        {"+1.5.3", "~1.2.3", false},
    }));
    INFO("Checking whether range '" << lhs << "' has overlap with '" << rhs << "'");
    CHECK(semver::range::parse(lhs).overlaps(semver::range::parse(rhs)) == should_overlap);
    INFO("Checking reflexive...");
    CHECK(semver::range::parse(rhs).overlaps(semver::range::parse(lhs)) == should_overlap);
}