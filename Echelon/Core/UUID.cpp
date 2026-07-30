#include "UUID.hpp"

#include <random>
#include <unordered_map>

namespace Echelon {

	static std::random_device s_RandomDevice;
	static std::mt19937 s_Engine(s_RandomDevice());
	static uuids::uuid_random_generator s_UUIDGenerator(s_Engine);
	static uuids::uuid_name_generator s_UUIDNameGenerator(uuids::uuid::from_string("6ba7b810-9dad-11d1-80b4-00c04fd430c8").value());

	UUID::UUID()
		: m_UUID(s_UUIDGenerator())
	{
	}

	UUID::UUID(const uuids::uuid& uuidBytes)
		: m_UUID(uuidBytes)
	{
	}

	UUID::UUID(const std::string& uuidString)
	{
		auto parsed = uuids::uuid::from_string(uuidString);
		m_UUID = parsed.has_value() ? *parsed : s_UUIDGenerator();
	}

	UUID UUID::Null()
	{
		return UUID(uuids::uuid{});
	}

	UUID UUID::FromName(const std::string& name)
	{
		static std::unordered_map<std::string, UUID> s_UUIDCache;

		auto it = s_UUIDCache.find(name);
		if (it != s_UUIDCache.end())
		{
			return it->second;
		}

		UUID uuid = s_UUIDNameGenerator(name);
		s_UUIDCache[name] = uuid;
		return uuid;
	}

}
