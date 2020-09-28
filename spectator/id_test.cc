#include "../spectator/id.h"
#include <gtest/gtest.h>

namespace {
using spectator::Id;
using spectator::Tags;

TEST(Id, Create) {
  Id id{"foo", Tags{}};
  EXPECT_EQ(id.Name(), "foo");

  Id id_tags{"name", Tags{{"k", "v"}, {"k1", "v1"}}};
  EXPECT_EQ(id_tags.GetTags().size(), 2);
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
