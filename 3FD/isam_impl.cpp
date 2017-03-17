#include "stdafx.h"
#include "isam_impl.h"

namespace _3fd
{
namespace isam
{
    const uint8_t NotNull(0x1);
    const uint8_t MultiValue(0x2);
    const uint8_t AutoIncrement(0x4);
    const uint8_t Sparse(0x8);
    const uint8_t Primary(0x10);
    const uint8_t Clustered(0x10);
    const uint8_t Unique(0x20);

	DataType ResolveDataType(bool value) { return DataType::Boolean; }
	DataType ResolveDataType(uint8_t value) { return DataType::UByte; }
	DataType ResolveDataType(uint16_t value) { return DataType::UInt16; }
	DataType ResolveDataType(uint32_t value) { return DataType::UInt32; }
	DataType ResolveDataType(int16_t value) { return DataType::Int16; }
	DataType ResolveDataType(int32_t value) { return DataType::Int32; }
	DataType ResolveDataType(int64_t value) { return DataType::Int64; }
	DataType ResolveDataType(float value) { return DataType::Float32; }
	DataType ResolveDataType(double value) { return DataType::Float64; }

	/// <summary>
	/// Makes a NULL input parameter for any data type.
	/// </summary>
	/// <param name="dataType">The parameter data type.</param>
	/// <returns>A <see cref="GenericInputParam" /> struct referring a null pointer and with the given data type.</returns>
	GenericInputParam NullParameter(DataType dataType)
	{
		return GenericInputParam(nullptr, 0, dataType);
	}
		
	/// <summary>
	/// Makes a text input parameter from a string literal.
	/// A text and a large text are considered the same. However if the function expects a 
	/// regular blob and receives a large one instead, truncation will take place.
	/// </summary>
	/// <param name="value">The string value.</param>
	/// <returns>A <see cref="GenericInputParam" /> struct referring the given string.</returns>
	GenericInputParam AsInputParam(const char *value)
	{
		return GenericInputParam(const_cast<char *> (value), 
                                 strlen(value), 
                                 DataType::Text);
	}
		
	/// <summary>
	/// Makes a text input parameter from a wide string literal.
	/// A text and a large text are considered the same. However if the function expects a 
	/// regular blob and receives a large one instead, truncation will take place.
	/// </summary>
	/// <param name="value">The string value.</param>
	/// <returns>A <see cref="GenericInputParam" /> struct referring the given string.</returns>
	GenericInputParam AsInputParam(const wchar_t *value)
	{
		return GenericInputParam(const_cast<wchar_t *> (value), 
                                 wcslen(value) * sizeof (wchar_t), 
                                 DataType::Text);
	}
		
	/// <summary>
	/// Makes a text input parameter from a string.
	/// A text and a large text are considered the same. However if the function expects a 
	/// regular blob and receives a large one instead, truncation will take place.
	/// </summary>
	/// <param name="value">The string value.</param>
	/// <returns>A <see cref="GenericInputParam" /> struct referring the given string.</returns>
	template <> GenericInputParam AsInputParam(const string &value)
	{
		return GenericInputParam(const_cast<char *> (value.data()), 
                                 value.length(), 
                                 DataType::Text);
	}
		
	/// <summary>
	/// Makes a text input parameter from a wide string.
	/// A text and a large text are considered the same. However if the function expects a 
	/// regular blob and receives a large one instead, truncation will take place.
	/// </summary>
	/// <param name="value">The string value.</param>
	/// <returns>A <see cref="GenericInputParam" /> struct referring the given string.</returns>
	template <> GenericInputParam AsInputParam(const wstring &value)
	{
		return GenericInputParam(const_cast<wchar_t *> (value.data()), 
                                 value.length() * sizeof (wchar_t), 
                                 DataType::Text);
	}

	// Gets 1900-jan-01 00:00:00
	time_t GetEpoch1900()
	{
		tm base1900 = {};
		base1900.tm_mday = 1;
		return mktime(&base1900);
	}

	/// <summary>
	/// Makes a 'date time' input parameter from a <see cref="time_t" /> value.
	/// </summary>
	/// <param name="daysSince1900">A floating point variable which will store the <see cref="time_t" /> value as fractional days since 1900.</param>
	/// <param name="value">The <see cref="time_t" /> value.</param>
	/// <returns>A <see cref="GenericInputParam" /> struct referring the given floating point value.</returns>
	GenericInputParam AsInputParam(double &daysSince1900, time_t value)
	{
		static const time_t epoch1900 = GetEpoch1900();
		daysSince1900 = (value - epoch1900) / 86400.0;
		return GenericInputParam(&daysSince1900, sizeof daysSince1900, DataType::DateTime);
	}

	/// <summary>
	/// Makes a 'date time' input parameter from a <see cref="std::chrono::time_point{std::chrono::system_clock}" /> value.
	/// </summary>
	/// <param name="daysSince1900">A floating point variable which will store the <see cref="std::chrono::time_point{std::chrono::system_clock}" /> value as fractional days since 1900.</param>
	/// <param name="value">The <see cref="std::chrono::time_point{std::chrono::system_clock}" /> value.</param>
	/// <returns>A <see cref="GenericInputParam" /> struct referring the given floating point value.</returns>
	GenericInputParam AsInputParam(double &daysSince1900, const std::chrono::time_point<std::chrono::system_clock> &value)
	{
		using namespace std::chrono;
		time_t sinceEpoch = duration_cast<seconds>(value.time_since_epoch()).count();
		return AsInputParam(daysSince1900, sinceEpoch);
	}

	/// <summary>
	/// Translates a local structure into another type understood by the ISAM implementation.
	/// </summary>
	/// <param name="indexes">An array of definitions for indexes.</param>
	/// <param name="jetIndexes">An array of definitions for indexes, as understood by the ISAM implementation.</param>
	void TranslateStructures(const std::vector<ITable::IndexDefinition> &indexes, 
								std::vector<JET_INDEXCREATE_X> &jetIndexes)
	{
		jetIndexes.clear();
		jetIndexes.reserve(indexes.size());

		// Translate the indexes definitions into the ESENT API expected structures:
		for(auto &idx : indexes)
		{
			JET_INDEXCREATE_X jetIdx = {};
			jetIdx.cbStruct = sizeof jetIdx;
			jetIdx.szIndexName = const_cast<wchar_t *> (idx.name.c_str());
			jetIdx.szKey = const_cast<wchar_t *> (idx.keys.data());
			jetIdx.cbKey = idx.keys.size() * sizeof(wchar_t);
			jetIdx.ulDensity = 80;
			jetIdx.cbVarSegMac = 0;
			jetIdx.grbit = JET_bitIndexCrossProduct;

#ifndef _3FD_PLATFORM_WINRT
			jetIdx.lcid = 1033;
#endif
			if(idx.primary)
				jetIdx.grbit |= JET_bitIndexPrimary;

			if(idx.unique)
				jetIdx.grbit |= JET_bitIndexUnique;

			jetIndexes.emplace_back(jetIdx);
		}
	}

}// end namespace isam
}// end namespace _3fd
