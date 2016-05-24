/* RawFile.h
 *
 * Copyright (C) 2016 Thermo Fisher Scientific
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#pragma once

#ifdef _MSC_VER
#define THERMO __declspec(dllexport)
#else
#define THERMO
#endif

#define luaD_setNumber(L, n, k) lua_pushnumber(L, n); lua_setfield(L, -2, k)
#define luaD_setString(L, s, k) lua_pushstring(L, s); lua_setfield(L, -2, k)
#define luaD_setBoolean(L, s, k) lua_pushboolean(L, s); lua_setfield(L, -2, k)
#define luaD_getNumber(L, field, value) lua_getfield(L, -1, field); if (lua_isnumber(L, -1)) value = lua_tonumber(L, -1); lua_pop(L, 1)
#define luaD_getLong(L, field, value) lua_getfield(L, -1, field); if (lua_isnumber(L, -1)) value = (long)lua_tonumber(L, -1); lua_pop(L, 1)
#define luaD_getString(L, field, value) lua_getfield(L, -1, field); if (lua_isstring(L, -1)) value = lua_tostring(L, -1); lua_pop(L, 1)
#define luaD_getBoolean(L, field, value) lua_getfield(L, -1, field); if (lua_isboolean(L, -1)) value = lua_toboolean(L, -1); lua_pop(L, 1)

#define RawFileVersion				"1.0.0"
#define RawFileType					"LuaRawFile.Rawfile"
#define checkRawFile(L)				*reinterpret_cast<RawFile**>(luaL_checkudata(L, 1, RawFileType))

// MS File Reader COM details
#define MSFR_XRAWFILE				"MSFileReader.XRawfile.1"
#define MSFR_CLSID					"1d23188d-53fe-4c25-b032-dc70acdbdc02"

// Foundation COM details
#define FOUNDATION_XRAWFILE			"XRawfile.XRawfile.1"
#define FOUNDATION_CLSID			"5fe970b2-29c3-11d3-811d-00104b304896"

#ifdef PERFER_MS_FILE_READER
#define RAW_FILE_INSTANCE			MSFR_XRAWFILE
#define RAW_FILE_INSTANCE_BACKUP	FOUNDATION_XRAWFILE
#else
#define RAW_FILE_INSTANCE			FOUNDATION_XRAWFILE
#define RAW_FILE_INSTANCE_BACKUP	MSFR_XRAWFILE
#endif

#include <lua.hpp>
#include <iostream>
#include <sstream>
#include <cctype>
#include <atlstr.h>
#include <sys/stat.h>

// This uses the MS File Reader type library, but works for Foundation as well
#import "XRawfile2.tlb" 

#if LUA_VERSION_NUM < 502
#define COMPAT52_IS_LUAJIT 1
#include "compat-5.2.h"
#endif

using namespace MSFileReaderLib;

namespace RawFile {

	static char const* Version = RawFileVersion;

	int __index(lua_State* L);

	bool FileExist(const char* filePath)
	{
		struct stat buffer;
		return (stat(filePath, &buffer) == 0);
	}

	typedef struct _ChroPeak
	{
		double dTime;
		double dIntensity;
	} ChroPeak;

	typedef struct _labelData
	{
		double Mass;
		double Intensity;
		double Resolution;
		double Baseline;
		double Noise;
		double Charge;
	} LabelData;

	typedef struct _labelDataFlags
	{
		unsigned char Saturated;
		unsigned char Fragmented;
		unsigned char Merged;
		unsigned char Exception;
		unsigned char Modified;
	} LabelFlags;

	typedef struct _datapeak
	{
		double Mass;
		double Intensity;
	} DataPeak;

	typedef struct RawFile
	{
		const char* FileName;
		bool IsOpen;
		int init;
		IXRawfile5Ptr comRawFile;
		RawFile(const char* filePath) {	
			CoInitialize(NULL);	

			// Try to create an instance of the preferred Rawfile
			HRESULT result = comRawFile.CreateInstance(RAW_FILE_INSTANCE);	
						
			if (result != 0)
			{
				// If that failed, try the backup instance
				result = comRawFile.CreateInstance(RAW_FILE_INSTANCE_BACKUP);			
			}

			init = result;

			IsOpen = false;
			FileName = filePath;
		}
		~RawFile() { comRawFile.Release(); CoUninitialize(); }		
	} RawFile;

	int Register(lua_State* L);
	int openRawFile(lua_State* L);
	int closeRawFile(lua_State* L);
	int rawFileToString(lua_State* L);
	int tuneData(lua_State* L);
	int scanTrailer(lua_State* L);
	int statusLog(lua_State* L);
	int scanFilter(lua_State* L);
	int scanHeader(lua_State* L);
	int newRawFile(lua_State* L);
	static int getMetaTable(lua_State* L) {luaL_getmetatable(L, RawFileType);	return 1;}
	int getInstrumentMethod(lua_State* L);
	int getRetentionTime(lua_State* L);
	int getScanNumberFromRT(lua_State* L);
	int getMSNOrder(lua_State* L);
	int hasCentroidData(lua_State* L);
	int getChroData(lua_State* L);
	int getSpectrumData(lua_State* L);
	int getLabelData(lua_State* L);
	int getInAcquisition(lua_State* L);
	int getPrecursorMass(lua_State* L);
	int getSegmentsForScanNumber(lua_State* L);
	int releaseRawfile(lua_State* L);

	static const struct luaL_Reg thermo_rawfile_m[] = {
		{ "New", newRawFile },
		{ "Open", openRawFile },
		{ "Close", closeRawFile },
		{ "GetTuneData", tuneData },
		{ "GetScanTrailer", scanTrailer },
		{ "GetStatusLog", statusLog },
		{ "GetScanFilter", scanFilter },
		{ "GetScanHeader", scanHeader },
		{ "GetInstrumentMethod", getInstrumentMethod },
		{ "GetRetentionTime", getRetentionTime },
		{ "GetScanNumberFromRT", getScanNumberFromRT },
		{ "GetMSNOrder", getMSNOrder },
		{ "HasCentroidData", hasCentroidData },
		{ "GetChroData", getChroData },
		{ "GetSpectrum", getSpectrumData },
		{ "GetLabelData", getLabelData },
		{ "GetPrecursorMass", getPrecursorMass },
		{ "InAcquisition", getInAcquisition },
		{ "GetRawFileMetaTable", getMetaTable },
		{ "GetNumSegments", getSegmentsForScanNumber },
		{ "__tostring", rawFileToString },
		{ "__gc", releaseRawfile },
		{ "__index", __index },
		{ NULL, NULL }
	};

}