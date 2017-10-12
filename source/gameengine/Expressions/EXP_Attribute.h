#ifndef __EXP_ATTRIBUTE_H__
#define __EXP_ATTRIBUTE_H__

#include <string>
#include "MT_Vector2.h"
#include "MT_Vector3.h"

class PyObjectPlus;
class PyObject;

class EXP_Attribute
{
public:
	enum Type
	{
		TYPE_BOOL = 0,
		TYPE_SHORT,
		TYPE_USHORT,
		TYPE_INT,
		TYPE_UINT,
		TYPE_FLOAT,
		TYPE_STRING,
		TYPE_VECTOR2,
		TYPE_VECTOR3,
		TYPE_CUSTOM
	};

	enum Access
	{
		READ_WRITE,
		READ_ONLY
	};

	enum GetSetFlags
	{
		GETSET_NONE = 0,
		GETSET_CHECK = (1 << 0),
		GETSET_CLAMP = (1 << 1),
		GETSET_CUSTOM = (1 << 2)
	};

	using CustomGetterFunction = void (*)(PyObjectPlus *, void *, const EXP_Attribute *);
	/// The second parameter can be cast to any basic type by casting the whole function.
	using CustomSetterFunction = int (*)(PyObjectPlus *, void *, const EXP_Attribute *);
	using CustomCheckFunction = int (*)(PyObjectPlus *, const EXP_Attribute *);

	using GetterFunction = PyObject *(*)(PyObject *, void *);
	using SetterFunction = int (*)(PyObject *, PyObject *, void *);

private:
	/// Name of the python class.
	const std::string m_className;
	/// Name of the python attribute.
	const std::string m_name;

	GetterFunction m_getter;
	SetterFunction m_setter;

/*	/// Type of value.
	Type m_type;
	/// Read/write access or read-only.
	Access m_access;
	/// Getter/setter flags.
	GetSetFlags m_flags;*/
	/// Use float for integer based types too.
	float m_borders[2];

	/// Attribute offset.
	intptr_t m_offset;

	CustomGetterFunction m_customGetter;
	CustomSetterFunction m_customSetter;
	CustomCheckFunction m_customCheck;

public:
	EXP_Attribute();

	EXP_Attribute(const std::string& className, const std::string& name, intptr_t offset, float borders[2],
			GetterFunction getter, SetterFunction setter,
			CustomGetterFunction customGetter, CustomSetterFunction customSetter, CustomCheckFunction customCheck);

	template <typename Type>
	Type *PtrFromOffset(PyObjectPlus *self)
	{
		return (Type *)((intptr_t)(self) + m_offset);
	}

	static bool IsValid(PyObjectPlus *self);

	template <typename Type>
	void PrintSetterError();

	void PrintError(const std::string& msg);
};

template <>
void EXP_Attribute::PrintSetterError<bool>()
{
	PrintError(" = bool: Excepted a boolean.");
}

template <>
void EXP_Attribute::PrintSetterError<int>()
{
	PrintError(" = int: Excepted a int.");
}

template <>
void EXP_Attribute::PrintSetterError<unsigned int>()
{
	PrintError(" = int: Excepted a int.");
}

template <>
void EXP_Attribute::PrintSetterError<short>()
{
	PrintError(" = int: Excepted a int.");
}

template <>
void EXP_Attribute::PrintSetterError<unsigned short>()
{
	PrintError(" = int: Excepted a int.");
}

template <>
void EXP_Attribute::PrintSetterError<float>()
{
	PrintError(" = float: Excepted a float.");
}

template <>
void EXP_Attribute::PrintSetterError<std::string>()
{
	PrintError(" = str: Excepted a string.");
}

template <>
void EXP_Attribute::PrintSetterError<MT_Vector2>()
{
	PrintError(" = Vector: Excepted a 2d vector.");
}

template <>
void EXP_Attribute::PrintSetterError<MT_Vector3>()
{
	PrintError(" = Vector: Excepted a 3d vector.");
}

#endif  // __EXP_ATTRIBUTE_H__
