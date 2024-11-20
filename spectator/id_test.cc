#include "../spectator/id.h"
#include <gtest/gtest.h>

namespace {

using spectator::Id;
using spectator::Tags;

TEST(Id, Create) {
  Id id{"foo", Tags{}};
  EXPECT_EQ(id.Name(), "foo");
  EXPECT_EQ(id.GetTags().size(), 0);
  EXPECT_EQ(fmt::format("{}", id), "Id(name=foo, tags=[])");

  Id id_tags_single{"name", Tags{{"k", "v"}}};
  EXPECT_EQ(id_tags_single.Name(), "name");
  EXPECT_EQ(id_tags_single.GetTags().size(), 1);
  EXPECT_EQ(fmt::format("{}", id_tags_single), "Id(name=name, tags=[k=v])");

  Id id_tags_multiple{"name", Tags{{"k", "v"}, {"k1", "v1"}}};
  EXPECT_EQ(id_tags_multiple.Name(), "name");
  EXPECT_EQ(id_tags_multiple.GetTags().size(), 2);

  EXPECT_EQ(fmt::format("{}", id_tags_multiple), "Id(name=name, tags=[k=v, k1=v1])");

  std::shared_ptr<Id> id_of{Id::of("name", Tags{{"k", "v"}, {"k1", "v1"}})};
  EXPECT_EQ(id_of->Name(), "name");
  EXPECT_EQ(id_of->GetTags().size(), 2);
  EXPECT_EQ(fmt::format("{}", *id_of), "Id(name=name, tags=[k=v, k1=v1])");
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
