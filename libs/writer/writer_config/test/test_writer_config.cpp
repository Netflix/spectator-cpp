#include <libs/writer/writer_config/include/writer_config.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cstdlib>

// Helper to temporarily set an environment variable for testing
class EnvironmentVariableSetter
{
  public:
    EnvironmentVariableSetter(const std::string &name, const std::string &value) : m_name(name)
    {
        // Store original value (might be nullptr)
        m_originalValue = std::getenv(name.c_str());

        // Set the new value
        setenv(name.c_str(), value.c_str(), 1);
    }

    ~EnvironmentVariableSetter()
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

// Helper to temporarily unset an environment variable for testing
class EnvironmentVariableUnset
{
  public:
    EnvironmentVariableUnset(const std::string &name) : m_name(name)
    {
        // Store original value (might be nullptr)
        m_originalValue = std::getenv(name.c_str());

        // Always unset regardless of whether it was set before
        unsetenv(name.c_str());
    }

    ~EnvironmentVariableUnset()
    {
        // Only restore if there was an original value
        if (m_originalValue)
            setenv(m_name.c_str(), m_originalValue, 1);
    }

  private:
    std::string m_name;
    const char *m_originalValue;
};

// Test fixture for WriterConfig tests
class WriterConfigTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Ensure environment variable is unset before each test
        unsetenv("SPECTATOR_OUTPUT_LOCATION");
    }
};

// Test basic writer type initialization
TEST_F(WriterConfigTest, BasicWriterTypes)
{
    

    // Test "memory" type
    {
        WriterConfig config(WriterTypes::Memory);
        EXPECT_EQ(config.GetType(), WriterType::Memory);
        EXPECT_EQ(config.GetLocation(), DefaultLocations::NoLocation);
    }

    // Test "udp" type
    {
        WriterConfig config(WriterTypes::UDP);
        EXPECT_EQ(config.GetType(), WriterType::UDP);
        EXPECT_EQ(config.GetLocation(), DefaultLocations::UDP);
    }

    // Test "unix" type
    {
        WriterConfig config(WriterTypes::Unix);
        EXPECT_EQ(config.GetType(), WriterType::Unix);
        EXPECT_EQ(config.GetLocation(), DefaultLocations::UDS);
    }
}

// Test URL-based writer initialization
TEST_F(WriterConfigTest, URLBasedWriterTypes)
{

    // Test UDP URL
    {
        const std::string udpUrl = std::string(WriterTypes::UDPURL) + "192.168.1.100:8125";
        WriterConfig config(udpUrl);
        EXPECT_EQ(config.GetType(), WriterType::UDP);
        EXPECT_EQ(config.GetLocation(), udpUrl);
    }

    // Test Unix domain socket URL
    {
        const std::string unixUrl = std::string(WriterTypes::UnixURL) + "/var/run/custom/socket.sock";
        WriterConfig config(unixUrl);
        EXPECT_EQ(config.GetType(), WriterType::Unix);
        EXPECT_EQ(config.GetLocation(), unixUrl);
    }
}

// Test environment variable override
TEST_F(WriterConfigTest, EnvironmentVariableOverride)
{
    // Test environment variable overriding constructor parameter
    {
        EnvironmentVariableSetter setter("SPECTATOR_OUTPUT_LOCATION", WriterTypes::Memory);
        WriterConfig config(WriterTypes::UDP); // This should be ignored due to env var
        EXPECT_EQ(config.GetType(), WriterType::Memory);
        EXPECT_EQ(config.GetLocation(), DefaultLocations::NoLocation);
    }

    
    // Test with environment variable unset
    {
        EnvironmentVariableUnset unset("SPECTATOR_OUTPUT_LOCATION");
        WriterConfig config(WriterTypes::Memory);
        EXPECT_EQ(config.GetType(), WriterType::Memory);
        EXPECT_EQ(config.GetLocation(), DefaultLocations::NoLocation);
    }
}

// Test invalid writer type handling
TEST_F(WriterConfigTest, InvalidWriterType)
{
    // Test invalid type from constructor
    EXPECT_THROW({ WriterConfig config("invalid_type"); }, std::invalid_argument);

    // Test invalid type from environment variable
    {
        EnvironmentVariableSetter setter("SPECTATOR_OUTPUT_LOCATION", "invalid_env_value");
        EXPECT_THROW(
            {
                WriterConfig config("none"); // This should be ignored, env var used instead
            },
            std::invalid_argument);
    }
}

// Test case for edge cases
TEST_F(WriterConfigTest, EdgeCases)
{
    // Test empty string
    EXPECT_THROW({ WriterConfig config(""); }, std::invalid_argument);

    // Test with just URL scheme but no path
    {
        WriterConfig config("udp://");
        EXPECT_EQ(config.GetType(), WriterType::UDP);
        EXPECT_EQ(config.GetLocation(), "udp://");
    }
}