#pragma once

#include <libs/writer/writer_types/include/writer_types.h>

#include <string>
#include <stdexcept>

class WriterConfig
{
	public:
		WriterConfig(std::string type);

		const WriterType &GetType() const noexcept { return m_type; };

		const std::string &GetLocation() const noexcept { return m_location;};

	private:
		WriterType m_type;
		std::string m_location;
};