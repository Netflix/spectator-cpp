#include "../spectator/id.h"
#include <gtest/gtest.h>

namespace {
using spectator::Id;
using spectator::Tags;

TEST(Id, Create) {
  Id id{"foo", Tags{}};
  EXPECT_EQ(id.Name(), "foo");
  EXPECT_EQ(id.GetTags().size(), 0);

  Id id_tags{"name", Tags{{"k", "v"}, {"k1", "v1"}}};
  EXPECT_EQ(id_tags.Name(), "name");
  EXPECT_EQ(id_tags.GetTags().size(), 2);

  std::shared_ptr<Id> id_of{Id::of("name", Tags{{"k", "v"}, {"k1", "v1"}})};
  EXPECT_EQ(id_of->Name(), "name");
  EXPECT_EQ(id_of->GetTags().size(), 2);
  fmt::format("{}", id);
}

TEST(Id, Tags) {
  Id id{"foo", Tags{}};
  auto withTag = id.WithTag("k", "v");
  Tags tags{{"k", "v"}};
  EXPECT_EQ(tags, withTag->GetTags());

  auto withStat = withTag->WithStat("count");
  Tags tagsWithStat{{"k", "v"}, {"statistic", "count"}};
  EXPECT_EQ(tagsWithStat, withStat->GetTags());
}
}  // namespace
