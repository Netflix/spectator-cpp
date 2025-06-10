#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <libs/config/include/config.h>

// Helper to temporarily set an environment variable for testing
class EnvVarSetter
{
  public:
    EnvVarSetter(const std::string &name, const std::string &value) : m_name(name)
    {
        // Store original value (might be nullptr)
        m_originalValue = std::getenv(name.c_str());

        // Set the new value
        setenv(name.c_str(), value.c_str(), 1);
    }

    ~EnvVarSetter()
    {
        // Restore original state
        if (m_originalValue)
            setenv(m_name.c_str(), m_originalValue, 1);
        else
            unsetenv(m_name.c_str());
    }

  private:
    std::string m_name;
    const char *m_originalValue;
};

// Helper to temporarily unset an environment variable
class EnvVarUnset
{
  public:
    EnvVarUnset(const std::string &name) : m_name(name)
    {
        // Store original value (might be nullptr)
        m_originalValue = std::getenv(name.c_str());

        // Always unset regardless of whether it was set before
        unsetenv(name.c_str());
    }

    ~EnvVarUnset()
    {
        // Only restore if there was an original value
        if (m_originalValue)
            setenv(m_name.c_str(), m_originalValue, 1);
    }

  private:
    std::string m_name;
    const char *m_originalValue;
};

class ConfigTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Ensure environment variables are unset before each test
        unsetenv("TITUS_CONTAINER_NAME");
        unsetenv("TITUS_PROCESS_NAME");
    }
};

// Test initialization with different writer configs
TEST_F(ConfigTest, WriterConfigInitialization)
{
    // Test with memory writer
    {
        WriterConfig writerConfig(WriterTypes::Memory);
        Config config(writerConfig);

        EXPECT_EQ(config.GetLocation(), "");
        EXPECT_EQ(config.GetWriterType(), WriterType::Memory);
        EXPECT_TRUE(config.GetExtraTags().empty());
    }

    // Test with UDP writer
    {
        WriterConfig writerConfig(WriterTypes::UDP);
        Config config(writerConfig);

        EXPECT_EQ(config.GetLocation(), DefaultLocations::UDP);
        EXPECT_EQ(config.GetWriterType(), WriterType::UDP);
    }
}

// Test extra tags handling
TEST_F(ConfigTest, ExtraTags)
{
    WriterConfig writerConfig(WriterTypes::Memory);

    // Empty tags
    {
        Config config(writerConfig, {});
        EXPECT_TRUE(config.GetExtraTags().empty());
    }

    // Valid tags
    {
        std::unordered_map<std::string, std::string> tags = {{"app", "test-app"}, {"env", "testing"}, {"region", "us-east-1"}};

        Config config(writerConfig, tags);

        EXPECT_EQ(config.GetExtraTags().size(), 3);
        EXPECT_EQ(config.GetExtraTags().at("app"), "test-app");
        EXPECT_EQ(config.GetExtraTags().at("env"), "testing");
        EXPECT_EQ(config.GetExtraTags().at("region"), "us-east-1");
    }

    // Invalid tags (empty keys or values should be ignored)
    {
        std::unordered_map<std::string, std::string> tags = {{"valid", "value"}, {"", "empty-key"}, {"empty-value", ""}};

        Config config(writerConfig, tags);

        EXPECT_EQ(config.GetExtraTags().size(), 1);
        EXPECT_EQ(config.GetExtraTags().at("valid"), "value");
        EXPECT_FALSE(config.GetExtraTags().count(""));
        EXPECT_FALSE(config.GetExtraTags().count("empty-value"));
    }
}

// Test environment variable integration
TEST_F(ConfigTest, EnvironmentVariables)
{
    WriterConfig writerConfig(WriterTypes::Memory);

    // No environment variables
    {
        EnvVarUnset container("TITUS_CONTAINER_NAME");
        EnvVarUnset process("TITUS_PROCESS_NAME");

        Config config(writerConfig);
        EXPECT_TRUE(config.GetExtraTags().empty());
    }

    // With container name
    {
        EnvVarSetter container("TITUS_CONTAINER_NAME", "test-container");
        EnvVarUnset process("TITUS_PROCESS_NAME");

        Config config(writerConfig);

        EXPECT_EQ(config.GetExtraTags().size(), 1);
        EXPECT_EQ(config.GetExtraTags().at("nf.container"), "test-container");
    }

    // With process name
    {
        EnvVarUnset container("TITUS_CONTAINER_NAME");
        EnvVarSetter process("TITUS_PROCESS_NAME", "test-process");

        Config config(writerConfig);

        EXPECT_EQ(config.GetExtraTags().size(), 1);
        EXPECT_EQ(config.GetExtraTags().at("nf.process"), "test-process");
    }

    // With both environment variables
    {
        EnvVarSetter container("TITUS_CONTAINER_NAME", "test-container");
        EnvVarSetter process("TITUS_PROCESS_NAME", "test-process");

        Config config(writerConfig);

        EXPECT_EQ(config.GetExtraTags().size(), 2);
        EXPECT_EQ(config.GetExtraTags().at("nf.container"), "test-container");
        EXPECT_EQ(config.GetExtraTags().at("nf.process"), "test-process");
    }
}

// Test merging of environment variables and explicit tags
TEST_F(ConfigTest, MergingTags)
{
    WriterConfig writerConfig(WriterTypes::Memory);

    // Environment variables with additional tags
    {
        EnvVarSetter container("TITUS_CONTAINER_NAME", "test-container");
        EnvVarSetter process("TITUS_PROCESS_NAME", "test-process");

        std::unordered_map<std::string, std::string> tags = {{"custom", "value"}, {"env", "test"}};

        Config config(writerConfig, tags);

        EXPECT_EQ(config.GetExtraTags().size(), 4);
        EXPECT_EQ(config.GetExtraTags().at("nf.container"), "test-container");
        EXPECT_EQ(config.GetExtraTags().at("nf.process"), "test-process");
        EXPECT_EQ(config.GetExtraTags().at("custom"), "value");
        EXPECT_EQ(config.GetExtraTags().at("env"), "test");
    }

    // Override environment variables with explicit tags
    {
        EnvVarSetter container("TITUS_CONTAINER_NAME", "test-container");

        std::unordered_map<std::string, std::string> tags = {{"nf.container", "override-container"}};

        Config config(writerConfig, tags);

        EXPECT_EQ(config.GetExtraTags().size(), 1);
        EXPECT_EQ(config.GetExtraTags().at("nf.container"), "override-container");
    }
}
