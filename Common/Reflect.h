#pragma once

// Self-describing, tagged serialization driven by a per-struct describe().
//
// A type opts in by providing a member template:
//
//   struct Foo {
//       float bar = 0;
//       Baz   baz;   // nested describable
//       Quux  quux;  // trivially-copyable, no describe() -> stored as a blob
//       template <typename Ar> void describe(Ar& ar) {
//           ar.field("bar", bar);
//           ar.field("baz", baz);
//           ar.field("quux", quux);
//       }
//   };
//
// The same describe() serves both save and load. Each field is written with its
// name, type tag, and length, so the format tolerates fields being added,
// removed, or reordered: unknown fields are skipped, absent fields keep their
// in-memory default. Scalars are stored native-endian.

#include <Rush/MathTypes.h>
#include <Rush/UtilArray.h>
#include <Rush/UtilFile.h>
#include <Rush/UtilLog.h>
#include <Rush/UtilString.h>

#include <cstring>
#include <type_traits>
#include <utility>

namespace Rush
{
namespace Reflect
{

enum class FieldType : u8
{
	Int    = 1, // u8 info (low 7 bits = byte size, high bit = signed), then size bytes
	Float  = 2, // u8 size (4 or 8), then size bytes
	Vec    = 3, // u8 count (2..4), then count*4 bytes
	Blob   = 4, // u32 size, then size bytes; validated by size only on load
	Struct = 5, // u32 size, then size bytes (nested field stream)
};

template <typename T>
inline constexpr bool isVecType = std::is_same_v<T, Vec2> || std::is_same_v<T, Vec3> || std::is_same_v<T, Vec4>;

template <typename T>
inline constexpr u8 vecComponents = std::is_same_v<T, Vec2> ? 2 : (std::is_same_v<T, Vec3> ? 3 : 4);

class WriteArchive
{
public:
	DynamicArray<u8> buffer;

	template <typename T>
	void field(const char* name, T& value)
	{
		writeName(name);
		writeValue(value);
	}

private:
	void writeName(const char* name)
	{
		const u16 len = u16(strlen(name));
		appendRaw(&len, sizeof(len));
		appendRaw(name, len);
	}

	void appendRaw(const void* data, size_t size)
	{
		const size_t offset = buffer.size();
		buffer.resize(offset + size);
		memcpy(buffer.data() + offset, data, size);
	}

	void appendTag(FieldType tag) { buffer.push_back(u8(tag)); }

	template <typename T>
	void writeInt(const T& value)
	{
		appendTag(FieldType::Int);
		const u8 info = u8(sizeof(T)) | (std::is_signed_v<T> ? 0x80 : 0);
		buffer.push_back(info);
		appendRaw(&value, sizeof(T));
	}

	template <typename T>
	void writeValue(T& value)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			const u8 v = value ? 1 : 0;
			writeInt(v);
		}
		else if constexpr (std::is_enum_v<T>)
		{
			const auto v = static_cast<std::underlying_type_t<T>>(value);
			writeInt(v);
		}
		else if constexpr (std::is_integral_v<T>)
		{
			writeInt(value);
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			appendTag(FieldType::Float);
			buffer.push_back(u8(sizeof(T)));
			appendRaw(&value, sizeof(T));
		}
		else if constexpr (isVecType<T>)
		{
			appendTag(FieldType::Vec);
			buffer.push_back(vecComponents<T>);
			appendRaw(&value.x, sizeof(float) * vecComponents<T>);
		}
		else if constexpr (requires { value.describe(*this); })
		{
			appendTag(FieldType::Struct);
			const size_t sizePos = buffer.size();
			const u32    placeholder = 0;
			appendRaw(&placeholder, sizeof(placeholder));
			const size_t start = buffer.size();
			value.describe(*this);
			const u32 size = u32(buffer.size() - start);
			memcpy(buffer.data() + sizePos, &size, sizeof(size));
		}
		else
		{
			static_assert(std::is_trivially_copyable_v<T>, "Field type is not serializable");
			static_assert(!requires(std::remove_cvref_t<T>& v) { v.describe(std::declval<WriteArchive&>()); },
			    "describable type reached the blob path (likely accessed as const)");
			appendTag(FieldType::Blob);
			const u32 size = u32(sizeof(T));
			appendRaw(&size, sizeof(size));
			appendRaw(&value, sizeof(T));
		}
	}
};

class ReadArchive
{
public:
	// Reads a struct field stream occupying [begin, end) of stream.
	ReadArchive(DataStream& stream, u64 begin, u64 end) : m_stream(stream) { m_valid = scan(begin, end); }

	bool valid() const { return m_valid; }

	template <typename T>
	void field(const char* name, T& value)
	{
		const Entry* e = find(name);
		if (e)
		{
			readValue(*e, value);
		}
	}

private:
	struct Entry
	{
		String    name;
		FieldType tag;
		u64       offset; // first payload byte the reader consumes
		u64       size;   // bytes the reader may consume
	};

	DataStream&       m_stream;
	DynamicArray<Entry> m_entries;
	bool                m_valid = false;

	const Entry* find(const char* name) const
	{
		for (const Entry& e : m_entries)
		{
			if (strcmp(e.name.c_str(), name) == 0)
			{
				return &e;
			}
		}
		return nullptr;
	}

	bool scan(u64 begin, u64 end)
	{
		m_stream.seek(begin);
		while (m_stream.tell() < end)
		{
			u16 nameLen = 0;
			if (m_stream.readT(nameLen) != sizeof(nameLen)) { return false; }

			String name;
			if (nameLen)
			{
				name.reset(nameLen);
				if (m_stream.read(name.data(), nameLen) != nameLen) { return false; }
			}

			u8 tagByte = 0;
			if (m_stream.readT(tagByte) != sizeof(tagByte)) { return false; }
			const FieldType tag = FieldType(tagByte);

			u64 offset = m_stream.tell();
			u64 size   = 0;
			switch (tag)
			{
			case FieldType::Int:
			case FieldType::Float:
			{
				u8 info = 0;
				if (m_stream.readT(info) != sizeof(info)) { return false; }
				size = 1 + (tag == FieldType::Int ? (info & 0x7f) : info);
				break;
			}
			case FieldType::Vec:
			{
				u8 count = 0;
				if (m_stream.readT(count) != sizeof(count)) { return false; }
				size = 1 + u64(count) * sizeof(float);
				break;
			}
			case FieldType::Blob:
			case FieldType::Struct:
			{
				u32 n = 0;
				if (m_stream.readT(n) != sizeof(n)) { return false; }
				offset = m_stream.tell(); // payload after the size field
				size   = n;
				break;
			}
			default:
				return false; // unknown tag: cannot determine size
			}

			if (offset + size > end) { return false; }
			m_entries.push_back(Entry{std::move(name), tag, offset, size});
			m_stream.seek(offset + size);
		}
		return true;
	}

	template <typename T>
	void readValue(const Entry& e, T& value)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			u64 raw = 0;
			if (readIntRaw(e, raw)) { value = raw != 0; }
		}
		else if constexpr (std::is_enum_v<T>)
		{
			u64 raw = 0;
			if (readIntRaw(e, raw)) { value = static_cast<T>(static_cast<std::underlying_type_t<T>>(raw)); }
		}
		else if constexpr (std::is_integral_v<T>)
		{
			u64 raw = 0;
			if (readIntRaw(e, raw)) { value = T(raw); }
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			if (e.tag != FieldType::Float) { return; }
			m_stream.seek(e.offset);
			u8 srcSize = 0;
			if (m_stream.readT(srcSize) != sizeof(srcSize)) { return; }
			if (srcSize == 4 && e.size >= 5)
			{
				float f = 0;
				if (m_stream.read(&f, 4) == 4) { value = T(f); }
			}
			else if (srcSize == 8 && e.size >= 9)
			{
				double d = 0;
				if (m_stream.read(&d, 8) == 8) { value = T(d); }
			}
		}
		else if constexpr (isVecType<T>)
		{
			if (e.tag != FieldType::Vec) { return; }
			m_stream.seek(e.offset);
			u8 count = 0;
			if (m_stream.readT(count) != sizeof(count) || count != vecComponents<T>) { return; }
			m_stream.read(&value.x, sizeof(float) * vecComponents<T>);
		}
		else if constexpr (requires { value.describe(*this); })
		{
			if (e.tag != FieldType::Struct) { return; }
			ReadArchive child(m_stream, e.offset, e.offset + e.size);
			if (child.valid()) { value.describe(child); }
		}
		else
		{
			static_assert(std::is_trivially_copyable_v<T>, "Field type is not serializable");
			static_assert(!requires(std::remove_cvref_t<T>& v) { v.describe(std::declval<ReadArchive&>()); },
			    "describable type reached the blob path (likely accessed as const)");
			if (e.tag == FieldType::Blob && e.size == sizeof(T))
			{
				m_stream.seek(e.offset);
				m_stream.read(&value, sizeof(T));
			}
		}
	}

	// Reads an Int field into a 64-bit value, sign-extending signed sources.
	bool readIntRaw(const Entry& e, u64& out)
	{
		if (e.tag != FieldType::Int) { return false; }
		m_stream.seek(e.offset);
		u8 info = 0;
		if (m_stream.readT(info) != sizeof(info)) { return false; }

		const u8   srcSize  = info & 0x7f;
		const bool isSigned = (info & 0x80) != 0;
		if (srcSize == 0 || srcSize > 8 || e.size < u64(1) + srcSize) { return false; }

		u64 raw = 0;
		if (m_stream.read(&raw, srcSize) != srcSize) { return false; }

		if (isSigned && srcSize < 8)
		{
			const u64 signBit = u64(1) << (srcSize * 8 - 1);
			if (raw & signBit)
			{
				raw |= ~((u64(1) << (srcSize * 8)) - 1); // sign-extend
			}
		}
		out = raw;
		return true;
	}
};

constexpr u32 kFileMagic     = 0x31435352; // "RSC1"
constexpr u32 kFormatVersion = 1;

struct FileHeader
{
	u32 magic;
	u32 formatVersion;
	u32 userVersion;
	u32 payloadSize;
};

// userVersion lets an app force-reject older files on incompatible semantic
// changes; layout changes (add/remove/reorder fields) need no bump.
template <typename T>
bool saveToFile(const char* path, u32 userVersion, T& root)
{
	WriteArchive ar;
	root.describe(ar);

	FileOut f(path);
	if (!f.valid())
	{
		RUSH_LOG_ERROR("Failed to open config for writing: '%s'", path);
		return false;
	}

	const FileHeader header = {kFileMagic, kFormatVersion, userVersion, u32(ar.buffer.size())};
	f.writeT(header);
	f.write(ar.buffer.data(), ar.buffer.size());
	return true;
}

// Returns false (leaving root untouched) if the file is missing or its header
// mismatches. Matched fields overwrite; fields absent from the file are kept.
template <typename T>
bool loadFromFile(const char* path, u32 userVersion, T& root)
{
	FileIn f(path);
	if (!f.valid())
	{
		return false;
	}

	FileHeader header = {};
	if (f.readT(header) != sizeof(header)
		|| header.magic != kFileMagic
		|| header.formatVersion != kFormatVersion
		|| header.userVersion != userVersion)
	{
		RUSH_LOG_ERROR("Ignoring incompatible config '%s'", path);
		return false;
	}

	if (f.length() != sizeof(header) + u64(header.payloadSize))
	{
		RUSH_LOG_ERROR("Ignoring truncated config '%s'", path);
		return false;
	}

	if (header.payloadSize == 0)
	{
		return true; // no fields stored; keep defaults
	}

	DynamicArray<u8> payload;
	payload.resize(header.payloadSize);
	if (f.read(payload.data(), header.payloadSize) != header.payloadSize)
	{
		return false;
	}

	MemDataStream stream(static_cast<const void*>(payload.data()), payload.size());
	ReadArchive   ar(stream, 0, payload.size());
	if (!ar.valid())
	{
		RUSH_LOG_ERROR("Ignoring corrupt config '%s'", path);
		return false;
	}

	root.describe(ar);
	return true;
}

} // namespace Reflect
} // namespace Rush
