#include <meter_id.h>

#include <gtest/gtest.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace spectator;

TEST(MeterIdTest, EqualsSameName)
{
    const MeterId id1("foo");
    const MeterId id2("foo");
    EXPECT_EQ(id1, id2);
}

TEST(MeterIdTest, EqualsSameTags)
{
    const MeterId id1("foo", {{"a", "1"}, {"b", "2"}, {"c", "3"}});
    const MeterId id2("foo", {{"c", "3"}, {"b", "2"}, {"a", "1"}});
    EXPECT_EQ(id1, id2);
}

TEST(MeterIdTest, HashSameTags)
{
    const MeterId id1("foo", {{"a", "1"}, {"b", "2"}, {"c", "3"}});
    const MeterId id2("foo", {{"c", "3"}, {"b", "2"}, {"a", "1"}});
    EXPECT_EQ(std::hash<MeterId>{}(id1), std::hash<MeterId>{}(id2));
}

TEST(MeterIdTest, HashIgnoresTagOrderFromDifferentConstructionPaths)
{
    const MeterId id1 = MeterId("foo", {{"a", "1"}}).WithTag("b", "2");
    const MeterId id2 = MeterId("foo", {{"b", "2"}}).WithTag("a", "1");
    EXPECT_EQ(id1, id2);
    EXPECT_EQ(std::hash<MeterId>{}(id1), std::hash<MeterId>{}(id2));

    const std::unordered_set<MeterId> ids = {id1, id2};
    EXPECT_EQ(1u, ids.size());
}

TEST(MeterIdTest, HashMatchesEqualityForAnyInsertionOrder)
{
    // The same tags inserted in opposite order into maps with different bucket counts, so the maps iterate differently.
    std::unordered_map<std::string, std::string> forward;
    forward.reserve(1);
    forward.emplace("a", "1");
    forward.emplace("b", "2");
    forward.emplace("c", "3");
    forward.emplace("d", "4");
    forward.emplace("e", "5");
    forward.emplace("f", "6");

    std::unordered_map<std::string, std::string> backward;
    backward.reserve(256);
    backward.emplace("f", "6");
    backward.emplace("e", "5");
    backward.emplace("d", "4");
    backward.emplace("c", "3");
    backward.emplace("b", "2");
    backward.emplace("a", "1");

    const MeterId id1("name", forward);
    const MeterId id2("name", backward);
    EXPECT_EQ(id1, id2);
    EXPECT_EQ(std::hash<MeterId>{}(id1), std::hash<MeterId>{}(id2));
}

TEST(MeterIdTest, HashDistinguishesDifferentIds)
{
    const std::hash<MeterId> hash;
    EXPECT_NE(hash(MeterId("foo")), hash(MeterId("bar")));
    EXPECT_NE(hash(MeterId("foo", {{"a", "1"}})), hash(MeterId("bar", {{"a", "1"}})));
    EXPECT_NE(hash(MeterId("foo", {{"a", "1"}})), hash(MeterId("foo", {{"a", "2"}})));
    EXPECT_NE(hash(MeterId("foo", {{"a", "1"}})), hash(MeterId("foo", {{"a", "1"}, {"b", "2"}})));
    // Key and value swapped, and values swapped between keys: a plain sum of key and value hashes would collide.
    EXPECT_NE(hash(MeterId("foo", {{"a", "b"}})), hash(MeterId("foo", {{"b", "a"}})));
    EXPECT_NE(hash(MeterId("foo", {{"a", "1"}, {"b", "2"}})), hash(MeterId("foo", {{"a", "2"}, {"b", "1"}})));
}

TEST(MeterIdTest, IllegalCharsAreReplaced)
{
    const MeterId id("test`!@#$%^&*()-=~_+[]{}\\|;:'\",<.>/?foo");
    EXPECT_EQ("test______^____-_~______________.___foo", id.GetSpectatordId());
}

TEST(MeterIdTest, IllegalCharsInTagsAreReplaced)
{
    const MeterId id("foo", {{"a b", "c/d"}});
    EXPECT_EQ("foo,a_b=c_d", id.GetSpectatordId());
}

TEST(MeterIdTest, ValidCharsAreKept)
{
    const MeterId id("azAZ09-._~^");
    EXPECT_EQ("azAZ09-._~^", id.GetSpectatordId());
}

TEST(MeterIdTest, EachNonAsciiByteIsReplaced)
{
    const MeterId id("caf\xc3\xa9");
    EXPECT_EQ("caf__", id.GetSpectatordId());
}

TEST(MeterIdTest, LookupTags)
{
    const MeterId id1("foo", {{"a", "1"}, {"b", "2"}, {"c", "3"}});
    const MeterId id2("foo", {{"c", "3"}, {"b", "2"}, {"a", "1"}});
    std::unordered_map<MeterId, std::string> d;
    d[id1] = "test";
    EXPECT_EQ("test", d[id2]);
}

TEST(MeterIdTest, Name)
{
    const MeterId id1("foo", {{"a", "1"}});
    EXPECT_EQ("foo", id1.GetName());
}

TEST(MeterIdTest, SpectatordId)
{
    MeterId id1("foo");
    EXPECT_EQ("foo", id1.GetSpectatordId());

    MeterId id2("bar", {{"a", "1"}});
    EXPECT_EQ("bar,a=1", id2.GetSpectatordId());

    // The order of tags follows the tag map's iteration order, which is unspecified, so accept either order.
    MeterId id3("baz", {{"a", "1"}, {"b", "2"}});
    EXPECT_TRUE(id3.GetSpectatordId() == "baz,a=1,b=2" || id3.GetSpectatordId() == "baz,b=2,a=1")
        << id3.GetSpectatordId();
}

TEST(MeterIdTest, SpectatordIdOmitsTagsThatValidationDrops)
{
    // Empty or whitespace-only keys and values are dropped from GetTags(), so they must not reach the wire either.
    const MeterId id("foo", {{"a", ""}, {"b", "2"}, {" ", "x"}, {"c", "  "}});
    const std::unordered_map<std::string, std::string> expected = {{"b", "2"}};
    EXPECT_EQ(expected, id.GetTags());
    EXPECT_EQ("foo,b=2", id.GetSpectatordId());
}

TEST(MeterIdTest, WithTagAndWithTagsDropTagsSetToBlank)
{
    const MeterId id("foo", {{"a", "1"}, {"b", "2"}, {"c", "3"}});

    const MeterId without_a = id.WithTag("a", "");
    const std::unordered_map<std::string, std::string> expected_without_a = {{"b", "2"}, {"c", "3"}};
    EXPECT_EQ(expected_without_a, without_a.GetTags());
    EXPECT_TRUE(without_a.GetSpectatordId() == "foo,b=2,c=3" || without_a.GetSpectatordId() == "foo,c=3,b=2")
        << without_a.GetSpectatordId();

    const MeterId without_bc = id.WithTags({{"b", " "}, {"c", ""}});
    const std::unordered_map<std::string, std::string> expected_without_bc = {{"a", "1"}};
    EXPECT_EQ(expected_without_bc, without_bc.GetTags());
    EXPECT_EQ("foo,a=1", without_bc.GetSpectatordId());
}

TEST(MeterIdTest, ToString)
{
    MeterId id1("foo");
    EXPECT_EQ("MeterId(name=foo, tags={})", id1.to_string());

    MeterId id2("bar", {{"a", "1"}});
    EXPECT_EQ("MeterId(name=bar, tags={'a': '1'})", id2.to_string());

    MeterId id3("baz", {{"a", "1"}, {"b", "2"}, {"c", "3"}});
    EXPECT_EQ("MeterId(name=baz, tags={'a': '1', 'b': '2', 'c': '3'})", id3.to_string());
}

TEST(MeterIdTest, Tags)
{
    const MeterId id1("foo", {{"a", "1"}});
    const std::unordered_map<std::string, std::string> expected = {{"a", "1"}};
    EXPECT_EQ(expected, id1.GetTags());
}

TEST(MeterIdTest, TagsDefensiveCopy)
{
    MeterId id1("foo", {{"a", "1"}});
    auto tags = id1.GetTags();
    tags["b"] = "2";
    std::unordered_map<std::string, std::string> expected = {{"a", "1"}, {"b", "2"}};
    EXPECT_EQ(expected, tags);
    std::unordered_map<std::string, std::string> expected_original = {{"a", "1"}};
    EXPECT_EQ(expected_original, id1.GetTags());
}

TEST(MeterIdTest, WithTagReturnsNewObject)
{
    MeterId id1("foo");
    MeterId id2 = id1.WithTag("a", "1");
    EXPECT_NE(id1, id2);
    std::unordered_map<std::string, std::string> empty;
    EXPECT_EQ(empty, id1.GetTags());
    std::unordered_map<std::string, std::string> expected = {{"a", "1"}};
    EXPECT_EQ(expected, id2.GetTags());
}

TEST(MeterIdTest, WithTagsReturnsNewObject)
{
    MeterId id1("foo");
    MeterId id2 = id1.WithTags({{"a", "1"}, {"b", "2"}});
    EXPECT_NE(id1, id2);
    std::unordered_map<std::string, std::string> empty;
    EXPECT_EQ(empty, id1.GetTags());
    std::unordered_map<std::string, std::string> expected = {{"a", "1"}, {"b", "2"}};
    EXPECT_EQ(expected, id2.GetTags());
}