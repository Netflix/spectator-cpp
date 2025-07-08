#include <writer_config.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cstdlib>
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

class WriterConfigTest : public testing::Test
{
   protected:
    EnvironmentVariableGuard envGuard{"SPECTATOR_OUTPUT_LOCATION"};

    void SetUp() override { envGuard.unsetValue(); }
};

TEST_F(WriterConfigTest, BasicWriterTypes)
{
    // Test "memory" type
    {
        const WriterConfig config(WriterTypes::Memory);
        EXPECT_EQ(config.GetType(), WriterType::Memory);
        EXPECT_EQ(config.GetLocation(), DefaultLocations::NoLocation);
    }

    // Test "udp" type
    {
        const WriterConfig config(WriterTypes::UDP);
        EXPECT_EQ(config.GetType(), WriterType::UDP);
        EXPECT_EQ(config.GetLocation(), DefaultLocations::UDP);
    }

    // Test "unix" type
    {
        const WriterConfig config(WriterTypes::Unix);
        EXPECT_EQ(config.GetType(), WriterType::Unix);
        EXPECT_EQ(config.GetLocation(), DefaultLocations::UDS);
    }
}

TEST_F(WriterConfigTest, URLBasedWriterTypes)
{
    // Test UDP URL
    {
        const std::string udpUrl = std::string(WriterTypes::UDPURL) + "192.168.1.100:8125";
        const WriterConfig config(udpUrl);
        EXPECT_EQ(config.GetType(), WriterType::UDP);
        EXPECT_EQ(config.GetLocation(), udpUrl);
    }

    // Test Unix domain socket URL
    {
        const std::string unixUrl = std::string(WriterTypes::UnixURL) + "/var/run/custom/socket.sock";
        const WriterConfig config(unixUrl);
        EXPECT_EQ(config.GetType(), WriterType::Unix);
        EXPECT_EQ(config.GetLocation(), unixUrl);
    }
}

TEST_F(WriterConfigTest, BufferingConstructor)
{
    
    const WriterConfig config(WriterTypes::UDP, 2048);
    EXPECT_EQ(config.GetType(), WriterType::UDP);
    EXPECT_EQ(config.GetBufferSize(), 2048);
}

TEST_F(WriterConfigTest, InvalidWriterType)
{
    EXPECT_THROW({ WriterConfig config("invalid_type"); }, std::runtime_error);
    EXPECT_THROW({ WriterConfig config(""); }, std::runtime_error);

    {
        envGuard.setValue("invalid_env_value");
        EXPECT_THROW({ WriterConfig config(WriterTypes::Memory); }, std::runtime_error);
    }
}

TEST_F(WriterConfigTest, EdgeCases)
{
    // Test with just URL scheme but no path
    {
        const WriterConfig config("udp://");
        EXPECT_EQ(config.GetType(), WriterType::UDP);
        EXPECT_EQ(config.GetLocation(), "udp://");
    }
}