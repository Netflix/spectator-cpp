#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <libs/config/config.h>
#include <optional>

// Enhanced helper to temporarily modify an environment variable for testing
class EnvironmentVariableGuard
{
   public:
    explicit EnvironmentVariableGuard(const std::string& name) : m_name(name)
    {
        if (const char* value = std::getenv(name.c_str()))
        {
            m_originalValue = value;
        }
    }

    void setValue(const std::string& value) const { setenv(m_name.c_str(), value.c_str(), 1); }

    void unsetValue() const { unsetenv(m_name.c_str()); }

    ~EnvironmentVariableGuard()
    {
        if (m_originalValue.has_value())
        {
            setenv(m_name.c_str(), m_originalValue->c_str(), 1);
        }
        else
        {
            unsetenv(m_name.c_str());
        }
    }

   private:
    std::string m_name;
    std::optional<std::string> m_originalValue;
};

class ConfigTest : public testing::Test
{
   protected:
    // Create guards for each environment variable
    EnvironmentVariableGuard containerGuard{"TITUS_CONTAINER_NAME"};
    EnvironmentVariableGuard processGuard{"TITUS_PROCESS_NAME"};

    void SetUp() override
    {
        // Ensure environment variables are unset before each test
        containerGuard.unsetValue();
        processGuard.unsetValue();
    }
};

// Test initialization with different writer configs
TEST_F(ConfigTest, WriterConfigInitialization)
{
    // Test with memory writer
    {
        WriterConfig writerConfig(WriterTypes::Memory);
        Config config(writerConfig);

        EXPECT_EQ(config.GetWriterLocation(), "");
        EXPECT_EQ(config.GetWriterType(), WriterType::Memory);
        EXPECT_TRUE(config.GetExtraTags().empty());
    }

    // Test with UDP writer
    {
        WriterConfig writerConfig(WriterTypes::UDP);
        Config config(writerConfig);

        EXPECT_EQ(config.GetWriterLocation(), DefaultLocations::UDP);
        EXPECT_EQ(config.GetWriterType(), WriterType::UDP);
    }

    // Test UDP URL
    {
        const std::string udpUrl = std::string(WriterTypes::UDPURL) + "192.168.1.100:8125";
        WriterConfig writerConfig(udpUrl);
        Config config(writerConfig);

        EXPECT_EQ(config.GetWriterType(), WriterType::UDP);
        EXPECT_EQ(config.GetWriterLocation(), udpUrl);
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
        std::unordered_map<std::string, std::string> tags = {
            {"app", "test-app"}, {"env", "testing"}, {"region", "us-east-1"}};

        Config config(writerConfig, tags);

        EXPECT_EQ(config.GetExtraTags().size(), 3);
        EXPECT_EQ(config.GetExtraTags().at("app"), "test-app");
        EXPECT_EQ(config.GetExtraTags().at("env"), "testing");
        EXPECT_EQ(config.GetExtraTags().at("region"), "us-east-1");
    }

    // Invalid tags (empty keys or values should be ignored)
    {
        std::unordered_map<std::string, std::string> tags = {
            {"valid", "value"}, {"", "empty-key"}, {"empty-value", ""}};

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

    // No environment variables - already unset in SetUp()
    {
        Config config(writerConfig);
        EXPECT_TRUE(config.GetExtraTags().empty());
    }

    // With container name
    {
        containerGuard.setValue("test-container");
        // Process already unset from SetUp()

        Config config(writerConfig);

        EXPECT_EQ(config.GetExtraTags().size(), 1);
        EXPECT_EQ(config.GetExtraTags().at("nf.container"), "test-container");
    }

    // With process name
    {
        containerGuard.unsetValue();
        processGuard.setValue("test-process");

        Config config(writerConfig);

        EXPECT_EQ(config.GetExtraTags().size(), 1);
        EXPECT_EQ(config.GetExtraTags().at("nf.process"), "test-process");
    }

    // With both environment variables
    {
        containerGuard.setValue("test-container");
        processGuard.setValue("test-process");

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
        containerGuard.setValue("test-container");
        processGuard.setValue("test-process");

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
        containerGuard.setValue("test-container");
        processGuard.unsetValue();

        std::unordered_map<std::string, std::string> tags = {{"nf.container", "override-container"}};

        Config config(writerConfig, tags);

        EXPECT_EQ(config.GetExtraTags().size(), 1);
        EXPECT_EQ(config.GetExtraTags().at("nf.container"), "override-container");
    }
}