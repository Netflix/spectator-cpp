#include <libs/meter/meter_id/include/meter_id.h>

#include <gtest/gtest.h>

TEST(MeterIdTest, EqualsSameName)
{
    MeterId id1("foo");
    MeterId id2("foo");
    EXPECT_EQ(id1, id2);
}

TEST(MeterIdTest, EqualsSameTags)
{
    MeterId id1("foo", {{"a", "1"}, {"b", "2"}, {"c", "3"}});
    MeterId id2("foo", {{"c", "3"}, {"b", "2"}, {"a", "1"}});
    EXPECT_EQ(id1, id2);
}

TEST(MeterIdTest, HashSameTags)
{
    MeterId id1("foo", {{"a", "1"}, {"b", "2"}, {"c", "3"}});
    MeterId id2("foo", {{"c", "3"}, {"b", "2"}, {"a", "1"}});
    EXPECT_EQ(std::hash<MeterId>{}(id1), std::hash<MeterId>{}(id2));
}

TEST(MeterIdTest, IllegalCharsAreReplaced)
{
    MeterId id("test`!@#$%^&*()-=~_+[]{}\\|;:'\",<.>/?foo");
    EXPECT_EQ("test______^____-_~______________.___foo", id.GetSpectatordId());
}

TEST(MeterIdTest, LookupTags)
{
    MeterId id1("foo", {{"a", "1"}, {"b", "2"}, {"c", "3"}});
    MeterId id2("foo", {{"c", "3"}, {"b", "2"}, {"a", "1"}});
    std::unordered_map<MeterId, std::string> d;
    d[id1] = "test";
    EXPECT_EQ("test", d[id2]);
}

TEST(MeterIdTest, Name)
{
    MeterId id1("foo", {{"a", "1"}});
    EXPECT_EQ("foo", id1.GetName());
}

TEST(MeterIdTest, SpectatordId)
{
    MeterId id1("foo");
    EXPECT_EQ("foo", id1.GetSpectatordId());

    MeterId id2("bar", {{"a", "1"}});
    EXPECT_EQ("bar,a=1", id2.GetSpectatordId());

    MeterId id3("baz", {{"a", "1"}, {"b", "2"}});
    EXPECT_EQ("baz,a=1,b=2", id3.GetSpectatordId());
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
    MeterId id1("foo", {{"a", "1"}});
    std::unordered_map<std::string, std::string> expected = {{"a", "1"}};
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