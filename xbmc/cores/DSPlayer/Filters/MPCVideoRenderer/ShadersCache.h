/*
 * the shader caching coming from https://github.com/Blinue/Magpie
 *
 * (C) 2022 Ti-BEN
 *
 * This file is part of MPC-BE.
 *
 * MPC-BE is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-BE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once


#include <vector>
#include <string>
#include <rpc.h>
#include <ppl.h>
#include "ppltasks.h"
#include <map>
#include <variant>
#include "bcrypt.h"
#include "include/parallel_hashmap/phmap.h"

class Hasher
{
public:
	static Hasher& GetInstance()
	{
		static Hasher instance;
		return instance;
	}

	bool Initialize();

	bool Hash(void* data, size_t len, std::vector<BYTE>& result);

	DWORD GetHashLength() { return _hashLen; }
private:
	~Hasher();

	BCRYPT_ALG_HANDLE _hAlg = NULL;
	DWORD _hashObjLen = 0;		// hash size of the hash object
	void* _hashObj = nullptr;	// store the hash object
	DWORD _hashLen = 0;			// size of hash result
	BCRYPT_HASH_HANDLE _hHash = NULL;
};

class ShaderCache
{
public:
	static ShaderCache& GetInstance() {
		static ShaderCache instance;
		return instance;
	}

	bool Load(const wchar_t* fileName, std::wstring_view hash, ShaderDesc& desc);

	void Save(const wchar_t* fileName, std::wstring_view hash, const ShaderDesc& desc);

	static std::wstring GetHash(std::string_view source,const phmap::flat_hash_map<std::wstring, float>* inlineParams = nullptr);
	static std::wstring GetHash(std::string& source, const phmap::flat_hash_map<std::wstring, float>* inlineParams = nullptr);

private:
	void _AddToMemCache(const std::wstring& cacheFileName, const ShaderDesc& desc);
	bool _LoadFromMemCache(const std::wstring& cacheFileName, ShaderDesc& desc);

	phmap::flat_hash_map<std::wstring, std::pair<ShaderDesc, UINT>> _memCache;

	static std::wstring _GetCacheFileName(const wchar_t* fileName, std::wstring_view hash);

	// extension de fichier cache?Compiled mpc-hc scaler
	static constexpr const wchar_t* _SUFFIX = L"mpcx";

	// cached version
	// It will be updated when the cache file structure changes, invalidating all old caches
	static constexpr const UINT _VERSION = 1;
	UINT _lastAccess = 0;
};
