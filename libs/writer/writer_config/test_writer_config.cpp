#include <writer_config.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chrono>
#include <cstdlib>
#include <optional>

using namespace spectator;

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

TEST_F(WriterConfigTest, BufferingDefaults)
{
    // Without buffering
    {
        const WriterConfig config(WriterTypes::UDP);
        EXPECT_EQ(config.GetBufferSize(), 0u);
        EXPECT_EQ(config.GetFlushInterval(), BufferingDefaults::FlushInterval);
        EXPECT_EQ(config.GetFullBufferPolicy(), BufferingDefaults::FullBuffer);
    }

    // With a buffer size only
    {
        const WriterConfig config(WriterTypes::UDP, 2048);
        EXPECT_EQ(config.GetFlushInterval(), std::chrono::milliseconds(1000));
        EXPECT_EQ(config.GetFullBufferPolicy(), FullBufferPolicy::Block);
    }
}

TEST_F(WriterConfigTest, FlushIntervalAndFullBufferPolicy)
{
    // Both given
    {
        const WriterConfig config(WriterTypes::Unix, 4096, std::chrono::milliseconds(250), FullBufferPolicy::Drop);
        EXPECT_EQ(config.GetType(), WriterType::Unix);
        EXPECT_EQ(config.GetBufferSize(), 4096u);
        EXPECT_EQ(config.GetFlushInterval(), std::chrono::milliseconds(250));
        EXPECT_EQ(config.GetFullBufferPolicy(), FullBufferPolicy::Drop);
    }

    // A flush interval alone leaves the default policy, and may be given in another unit
    {
        const WriterConfig config(WriterTypes::UDP, 2048, std::chrono::seconds(5));
        EXPECT_EQ(config.GetFlushInterval(), std::chrono::milliseconds(5000));
        EXPECT_EQ(config.GetFullBufferPolicy(), FullBufferPolicy::Block);
    }

    // The send policy
    {
        const WriterConfig config(WriterTypes::UDP, 2048, std::chrono::milliseconds(100), FullBufferPolicy::Send);
        EXPECT_EQ(config.GetFullBufferPolicy(), FullBufferPolicy::Send);
    }

    // The names of the policies, as they appear in the log
    EXPECT_EQ(FullBufferPolicyToString(FullBufferPolicy::Block), "block");
    EXPECT_EQ(FullBufferPolicyToString(FullBufferPolicy::Drop), "drop");
    EXPECT_EQ(FullBufferPolicyToString(FullBufferPolicy::Send), "send");
}

TEST_F(WriterConfigTest, InvalidFlushInterval)
{
    EXPECT_THROW({ WriterConfig config(WriterTypes::UDP, 2048, std::chrono::milliseconds(0)); }, std::runtime_error);
    EXPECT_THROW({ WriterConfig config(WriterTypes::UDP, 2048, std::chrono::milliseconds(-1)); }, std::runtime_error);

    // Without buffering there is nothing to flush, so the interval is not checked
    EXPECT_NO_THROW({ WriterConfig config(WriterTypes::UDP, 0, std::chrono::milliseconds(0)); });
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