#pragma once

#define NOMINMAX
#define WIN32_MEAN_AND_LEAN
#ifndef WINVER
#define WINVER 0x0605
#endif
#include <Windows.h>
#include <Shlobj.h>
#include <stdexcept>

namespace
{
	static std::string from_utf16(const std::wstring& str)
	{
		if (str.empty()) return {};
		int needed = WideCharToMultiByte(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0, nullptr, nullptr);
		if (needed == 0)
			throw std::runtime_error("WideCharToMultiByte failed");
		std::string result;
		result.resize(needed);
		needed = WideCharToMultiByte(CP_UTF8, 0, str.data(), (int)str.size(), &result[0], (int)result.size(), nullptr, nullptr);
		if (needed == 0)
			throw std::runtime_error("WideCharToMultiByte failed");
		return result;
	}

	static std::wstring to_utf16(const std::string& str)
	{
		if (str.empty()) return {};
		int needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
		if (needed == 0)
			throw std::runtime_error("MultiByteToWideChar failed");
		std::wstring result;
		result.resize(needed);
		needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &result[0], (int)result.size());
		if (needed == 0)
			throw std::runtime_error("MultiByteToWideChar failed");
		return result;
	}

	template<typename T>
	class ComPtr
	{
	public:
		ComPtr() { Ptr = nullptr; }
		ComPtr(const ComPtr& other) { Ptr = other.Ptr; if (Ptr) Ptr->AddRef(); }
		ComPtr(ComPtr&& move) { Ptr = move.Ptr; move.Ptr = nullptr; }
		~ComPtr() { reset(); }
		ComPtr& operator=(const ComPtr& other) { if (this != &other) { if (Ptr) Ptr->Release(); Ptr = other.Ptr; if (Ptr) Ptr->AddRef(); } return *this; }
		void reset() { if (Ptr) Ptr->Release(); Ptr = nullptr; }
		T* get() { return Ptr; }
		static IID GetIID() { return __uuidof(T); }
		void** InitPtr() { return (void**)TypedInitPtr(); }
		T** TypedInitPtr() { reset(); return &Ptr; }
		operator T* () const { return Ptr; }
		T* operator ->() const { return Ptr; }
		T* Ptr;
	};
}
