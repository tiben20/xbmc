/*
 * the shader caching coming from https://github.com/Blinue/Magpie
 *
 * (C) 2022-2024 Ti-BEN
 *
 * This file is part of KODI
 *
 * KODI is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * KODI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "ShadersLoader.h"
#include "ShadersCache.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <mutex>
#include <filesystem>
#include <regex>
#include "utils/CharsetConverter.h"
//#include "Utility.h"
//#include "FileUtility.h"
#include "filesystem/file.h"
#include "filesystem/Directory.h"
#include "filesystem/IFileTypes.h"
#include <bitset>
#include <unordered_set>
#include <charconv>
#include "ctype.h"
#include "helper.h"
#include "shaders.h"
#include "winternl.h"
#include <istream>
#include "YasHelper.h"
#include "d3dcompiler.h"
#pragma comment( lib, "bcrypt.lib" )

using namespace std::literals::string_literals;

struct HandleCloser { void operator()(HANDLE h) noexcept { assert(h != INVALID_HANDLE_VALUE); if (h) CloseHandle(h); } };

using ScopedHandle = std::unique_ptr<std::remove_pointer<HANDLE>::type, HandleCloser>;

static HANDLE SafeHandle(HANDLE h) noexcept { return (h == INVALID_HANDLE_VALUE) ? nullptr : h; }

bool ReadFile(const wchar_t* fileName, std::vector<BYTE>& result)
{
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
	ScopedHandle hFile(SafeHandle(CreateFile2(fileName, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr)));
#else
	ScopedHandle hFile(SafeHandle(CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr)));
#endif

	if (!hFile) {

		return false;
	}

	DWORD size = GetFileSize(hFile.get(), nullptr);
	result.resize(size);

	DWORD readed;
	if (!::ReadFile(hFile.get(), result.data(), size, &readed, nullptr)) {

		return false;
	}

	return true;
}

namespace yas::detail {

	// winrt::com_ptr<ID3DBlob>
	template<std::size_t F>
	struct serializer<
		type_prop::not_a_fundamental,
		ser_case::use_internal_serializer,
		F,
		
		Microsoft::WRL::ComPtr<ID3DBlob>
	> {
		template<typename Archive>
		static Archive& save(Archive& ar, const Microsoft::WRL::ComPtr<ID3DBlob>& blob) {
			uint32_t size = (uint32_t)blob->GetBufferSize();
			ar& size;

			ar.write(blob->GetBufferPointer(), size);

			return ar;
		}

		template<typename Archive>
		static Archive& load(Archive& ar, Microsoft::WRL::ComPtr<ID3DBlob>& blob) {
			uint32_t size = 0;
			ar& size;
			HRESULT hr = D3DCreateBlob(size, &blob);
			if (FAILED(hr)) {
				
				throw new std::exception();
			}

			ar.read(blob->GetBufferPointer(), size);

			return ar;
		}
	};

}

template<typename Archive>
void serialize(Archive& ar, ShaderParameterDesc& o) {
	ar& o.name& o.label& o.constant;
}


template<typename Archive>
void serialize(Archive& ar, ShaderIntermediateTextureDesc& o) {
	ar& o.format& o.name& o.source& o.sizeExpr;
}

template<typename Archive>
void serialize(Archive& ar, ShaderSamplerDesc& o) {
	ar& o.filterType& o.addressType& o.name;
}

template<typename Archive>
void serialize(Archive& ar, ShaderPassDesc& o) {
	ar& o.cso& o.inputs& o.outputs& o.numThreads[0] & o.numThreads[1] & o.numThreads[2] & o.blockSize& o.desc& o.isPSStyle;
}

template<typename Archive>
void serialize(Archive& ar, ShaderDesc& o) {
	ar& o.name& o.params& o.textures& o.samplers& o.passes& o.flags;
}

static constexpr const uint32_t MAX_CACHE_COUNT = 127;

static constexpr const uint32_t EFFECT_CACHE_VERSION = 13;

std::wstring ConvertFileName(const wchar_t* fileName)
{
	std::wstring file(fileName);

	// remove the path from the filename
	size_t pos = file.find_last_of('\\');
	if (pos != std::wstring::npos) {
		file.erase(0, pos + 1);
	}

	std::replace(file.begin(), file.end(), '.', '_');
	return file;
}


Hasher::~Hasher()
{
	if (_hAlg) {
		BCryptCloseAlgorithmProvider(_hAlg, 0);
	}
	if (_hashObj) {
		HeapFree(GetProcessHeap(), 0, _hashObj);
	}
	if (_hHash) {
		BCryptDestroyHash(_hHash);
	}
}

bool Hasher::Initialize()
{
	NTSTATUS status = BCryptOpenAlgorithmProvider(&_hAlg, BCRYPT_SHA1_ALGORITHM, NULL, 0);
	if (!NT_SUCCESS(status)) {
		
		return false;
	}

	ULONG result;

	status = BCryptGetProperty(_hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&_hashObjLen, sizeof(_hashObjLen), &result, 0);
	if (!NT_SUCCESS(status)) {
		
		return false;
	}

	_hashObj = HeapAlloc(GetProcessHeap(), 0, _hashObjLen);
	if (!_hashObj) {
		
		return false;
	}

	status = BCryptGetProperty(_hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&_hashLen, sizeof(_hashLen), &result, 0);
	if (!NT_SUCCESS(status))
	{
		
		return false;
	}

	status = BCryptCreateHash(_hAlg, &_hHash, (PUCHAR)_hashObj, _hashObjLen, NULL, 0, BCRYPT_HASH_REUSABLE_FLAG);
	if (!NT_SUCCESS(status))
	{
		
		return false;
	}

	
	return true;
}

bool Hasher::Hash(void* data, size_t len, std::vector<BYTE>& result) {
	result.resize(_hashLen);

	NTSTATUS status = BCryptHashData(_hHash, (PUCHAR)data, (ULONG)len, 0);
	if (!NT_SUCCESS(status)) {
		
		return false;
	}

	status = BCryptFinishHash(_hHash, result.data(), (ULONG)result.size(), 0);
	if (!NT_SUCCESS(status)) {
		
		return false;
	}

	return true;
}

std::wstring ShaderCache::_GetCacheFileName(const wchar_t* fileName, std::wstring_view hash)
{
	std::wstring fname = ConvertFileName(fileName);
	

	std::wstring fsuffix = _SUFFIX;

	return fmt::format(L"special://temp//cache//{}_{}.{}", fname, hash, fsuffix);
}

void ShaderCache::_AddToMemCache(const std::wstring& cacheFileName, const ShaderDesc& desc)
{
	_memCache[cacheFileName] = { desc, ++_lastAccess };

	if (_memCache.size() > MAX_CACHE_COUNT)
	{
		assert(_memCache.size() == MAX_CACHE_COUNT + 1);

		// clear half of the memory cache
		std::array<uint32_t, MAX_CACHE_COUNT + 1> access{};
		std::transform(_memCache.begin(), _memCache.end(), access.begin(),
			[](const auto& pair) {return pair.second.second; });

		auto midIt = access.begin() + access.size() / 2;
		std::nth_element(access.begin(), midIt, access.end());
		const uint32_t mid = *midIt;

		for (auto it = _memCache.begin(); it != _memCache.end();) {
			if (it->second.second < mid) {
				it = _memCache.erase(it);
			}
			else {
				++it;
			}
		}
		CLog::Log(LOGINFO,"Memory cache cleared");
	}
}

bool ShaderCache::_LoadFromMemCache(const std::wstring& cacheFileName, ShaderDesc& desc)
{
	auto it = _memCache.find(cacheFileName);
	if (it != _memCache.end()) {
		desc = it->second.first;
		it->second.second = ++_lastAccess;
		CLog::Log(LOGINFO, "_LoadFromMemCache {}",WToA(cacheFileName));
		return true;
	}
	return false;
}


bool ShaderCache::Load(const wchar_t* fileName, std::wstring_view hash, ShaderDesc& desc)
{
	std::wstring cacheFileName = _GetCacheFileName(fileName, hash);

	if (_LoadFromMemCache(cacheFileName, desc))
	{
		return true;
	}

	XFILE::CFile fle;
	
	if (!fle.Exists(WToA(cacheFileName)))
		return false;
	
	
	if (!fle.Open(WToA(cacheFileName)))
		return false;


	/*if (file.Open(pFileNameA))
	{
		filelength = file.GetLength();
		source.resize(file.GetLength() + 1);
		file.Read(&source[0], file.GetLength());
	}*/
	int64_t length = fle.GetLength();
	std::string pData;
	pData.resize(fle.GetLength() + 1);
	fle.Read(&pData[0], fle.GetLength());
	/*void* pData = malloc(length);
	if (fle.Read(pData, length) != length)
	{
		free(pData);
		return false;
	}*/
	std::vector<BYTE> buf(pData.begin(), pData.end());


	try {
		yas::mem_istream mi(buf.data(), buf.size());
		yas::binary_iarchive<yas::mem_istream, yas::binary> ia(mi);

		ia& desc;
	}
	catch (...) {
		desc = {};
		return false;
	}

	_AddToMemCache(cacheFileName, desc);

	
	return true;
}

void ShaderCache::Save(const wchar_t* fileName, std::wstring_view hash, const ShaderDesc& desc)
{


	// Format: HASH-VERSION-FL-{BODY}
	std::vector<BYTE> buf;
	buf.reserve(4096);
	try
	{
	  yas::vector_ostream os(buf);
		yas::binary_oarchive<yas::vector_ostream<BYTE>, yas::binary> oa(os);
		oa& desc;
	}
	catch (...) {
		
		return;
	}
	std::wstring cacheFileName = _GetCacheFileName(fileName, hash);
	XFILE::CFile fle;
	if (fle.OpenForWrite(WToA(cacheFileName)))
	{
		fle.Write(buf.data(), buf.size());
	}  

	_AddToMemCache(cacheFileName, desc);
	return;
	
	// fill HASH
	std::vector<BYTE> bufHash;
	if (!Hasher::GetInstance().Hash(buf.data() + Hasher::GetInstance().GetHashLength(),
		buf.size() - Hasher::GetInstance().GetHashLength(),
		bufHash
	))
	{
		
		return;
	}
	std::memcpy(buf.data(), bufHash.data(), bufHash.size());


	if (!XFILE::CDirectory::Exists("special://temp//cache"))
	{
		if (!XFILE::CDirectory::Create("special://temp//cache"))
			return;
	}
	else {
		// supprimer tous les caches de ce fichier
		std::wregex regex(fmt::format(L"^{}_[0-9,a-f]{{{}}}.{}$", ConvertFileName(fileName),
			Hasher::GetInstance().GetHashLength() * 2, _SUFFIX), std::wregex::optimize | std::wregex::nosubs);

		WIN32_FIND_DATA findData;
		HANDLE hFind = SafeHandle(FindFirstFile(L".\\cache\\*", &findData));
		if (hFind) {
			while (FindNextFile(hFind, &findData)) {
				if (lstrlenW(findData.cFileName) < 8) {
					continue;
				}

				// Regex matching filename
				if (!std::regex_match(findData.cFileName, regex)) {
					continue;
				}
				
				if (!DeleteFile((L".\\cache\\"s + findData.cFileName).c_str())) {
				}
			}
			FindClose(hFind);
		}
		else {
			
		}
	}

	

	
}
