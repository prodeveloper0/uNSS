#pragma once
//////////////////////////////
// Simple INI Parser
// Written by Samuel Lee(pr0ximo, prodeveloper0)
//
//
// Usage:
// sample.ini
// [Pri]
// ; This is my information
// name = "Samuel Lee"
// age = 25
// 
// [Sub]
// ; This is my status
// healthy = 20
// tired = 1000
// vision_left = 0.3
// vision_right = 0.4
//
//
// main.cpp
// Config cfg("sample.ini");
// // No errors!
// std::string name = cfg["Pri"]["name"];
// int heathy = cfg["Sub"]["healthy"];
// float vision_left = cfg["Sub"]["vision_left"];
//
// // Exceptions are throwed!
// int name_invalid_type = cfg["Pri"]["name"];
// 

#include <string>
#include <map>

#include <sstream>
#include <fstream>
#include <algorithm>


template<typename T = void>
class Config_
{
protected:
	static const char* ws;
	static const char* wsSection;
	static const char* wsProperty;

	inline std::string& rtrim(std::string& s, const char* t = ws)
	{
		s.erase(s.find_last_not_of(t) + 1);
		return s;
	}

	inline std::string& ltrim(std::string& s, const char* t = ws)
	{
		s.erase(0, s.find_first_not_of(t));
		return s;
	}

	inline std::string& trim(std::string& s, const char* t = ws)
	{
		return ltrim(rtrim(s, t), t);
	}

public:
	class Value
	{
	protected:
		template<typename U>
		static U lexical_cast(const std::string& s)
		{
			std::stringstream ss(s);

			U result;
			if((ss >> result).fail() || !(ss >> std::ws).eof())
				return U();

			return result;
		}

	public:
		template<typename U>
		operator U ()
		{
			return lexical_cast<U>(value);
		}

	public:
		std::string value;

	public:
		Value(const std::string& v = "")
			: value(v)
		{
		}

	public:
		bool has() const
		{
			return !value.empty();
		}

		template<typename U>
		bool has(const U& defaultValue)
		{
			if (!has()) {
				std::stringstream ss;
				ss << defaultValue;
				value = ss.str();
				return false;
			}
			return true;
		}

		Value& operator=(const std::string& v)
		{
			value = v;
			return *this;
		}

		Value& operator=(const Value& v)
		{
			value = v.value;
			return *this;
		}
	};

	class Property
	{
	public:
		std::map<std::string, Value> properties;

	public:
		void add(const std::string& k, const Value& v)
		{
			properties[k] = v;
		}

	public:
		Value& operator [] (const std::string& k)
		{
			auto it = properties.find(k);

			if(it == properties.cend())
			{
				// Create a new empty value instead of throwing
				properties[k] = Value();
				return properties[k];
			}

			return it->second;
		}

		const Value& operator [] (const std::string& k) const
		{
			auto it = properties.find(k);

			if(it == properties.cend())
			{
				static Value defaultValue;
				return defaultValue;
			}

			return it->second;
		}
	};

protected:
	std::map<std::string, Property> config;

public:
	Config_(const std::string& filename = "")
	{
		if(filename.size() == 0)
			return;

		read(filename);
	}

public:
	bool read(const std::string& filename)
	{
		std::ifstream ifs(filename);
		bool result = false;

		std::string section;
		std::string line;
		std::string key;
		std::string value;
		std::map<std::string, Property> tempConfig;

		// Is a file is opened successfully?
		if(!ifs.is_open())
			goto result;

		while(std::getline(ifs, line))
		{
			line = trim(line);

			// Is the current line a comment or blank line?
			if(line.size() == 0 || *line.cbegin() == ';')
				continue;

			// Is the current line a section contains brackets '[]'?
			if(*line.cbegin() == '[' || *line.cend() == ']')
			{
				section = trim(line, wsSection);
				if(section.size() == 0)
					goto result;

				continue;
			}

			// Is the current line a property?
			if(section.size() == 0)
				goto result;

			size_t kvOffset = line.find('=');
			if(kvOffset == std::string::npos)
				goto result;

			std::string tempKey = line.substr(0, kvOffset);
			std::string tempValue = line.substr(kvOffset + 1);
			key = trim(tempKey, wsProperty);
			value = trim(tempValue, wsProperty);

			if(key.size() == 0)
				goto result;

			tempConfig[section].add(key, value);
		}

		result = true;
		config = tempConfig;

		result:
		ifs.close();
		return result;
	}

public:
	Property& operator [] (const std::string& s)
	{
		auto it = config.find(s);

		if(it == config.cend())
		{
			// Create a new empty property instead of throwing
			config[s] = Property();
			return config[s];
		}

		return it->second;
	}

	const Property& operator [] (const std::string& s) const
	{
		auto it = config.find(s);

		if(it == config.cend())
		{
			static Property defaultProperty;
			return defaultProperty;
		}

		return it->second;
	}
};

template<>
inline const char* Config_<>::ws = " \t\n\r\f\v";
template<>
inline const char* Config_<>::wsSection = " \t\n\r\f\v[]";
template<>
inline const char* Config_<>::wsProperty = " \t\n\r\f\v\'\"";

class Config : public Config_<>
{
public:
	Config(const std::string filename = "")
		: Config_(filename)
	{
	}
};
