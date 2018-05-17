/// RawFile
//  @module	lrf

/* RawFile.cpp
 *
 * Copyright (C) 2016 Thermo Fisher Scientific
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include "RawFile.h"
#include "comutil.h"

namespace RawFile {

	static const struct luaL_Reg luaRawFile_l[] = {
		{ "New", newRawFile },	
		{ "GetRawFileMetaTable", getMetaTable },
		{ NULL, NULL }
	};
		
	extern "C" THERMO int luaopen_LuaRawFile_core(lua_State* L)
	{	
		Register(L);	

		luaL_newlib(L, luaRawFile_l);

		lua_pushstring(L, Version);
		lua_setfield(L, -2, "Version");

		return 1;
	}

	int __index(lua_State* L)
	{
		const char* key = luaL_checkstring(L, 2);

		// Get the metatable for this userdata
		lua_getmetatable(L, 1);

		// Attempt to get the field
		lua_getfield(L, -1, key);
		if (!lua_isnil(L, -1))
			return 1;

		// Get the user value stored with this userdata
		lua_getuservalue(L, 1);
		if (lua_isnil(L, -1))
			return 0;

		// Attempt to get the field
		lua_getfield(L, -1, key);
		if (!lua_isnil(L, -1))
			return 1;

		return 0;
	}


	bool is_number(const std::string& s)
	{
		std::string::const_iterator it = s.begin();
		while (it != s.end() && std::isdigit(*it) || *it == '.' ) ++it;
		return !s.empty() && it == s.end();
	}

	static int ListToTable(lua_State* L, VARIANT* items, int size)
	{
		lua_createtable(L, size, 0);

		BSTR* pItems = NULL;
		SAFEARRAY FAR* itemsSA = items->parray;
		SafeArrayAccessData(itemsSA, (void**)(&pItems));
		for (int i = 0; i < size; i++)
		{
			CStringA value = (CStringA)pItems[i];
			std::string sValue((LPCTSTR)value);
			lua_pushstring(L, sValue.c_str());
			lua_rawseti(L, -2, i + 1);
		}
		SafeArrayUnaccessData(itemsSA);
		SafeArrayDestroy(itemsSA);

		return 1;
	}

	static int MapToStack(lua_State* L, VARIANT* labels, VARIANT* values, int size)
	{
		lua_createtable(L, 0, size);
		BSTR* pLabels = NULL;
		BSTR* pValues = NULL;

		SAFEARRAY FAR* labelSA = labels->parray;
		SafeArrayAccessData(labelSA, (void**)(&pLabels));
		SAFEARRAY FAR* valueSA = values->parray;
		SafeArrayAccessData(valueSA, (void**)(&pValues));

		for (int i = 0; i < size; i++)
		{		
			lua_pushstring(L, (CStringA)pLabels[i]);

			CStringA value = (CStringA)pValues[i];
			std::string sValue((LPCTSTR)value);

			if (is_number(sValue))
			{
				lua_pushnumber(L, std::stod(sValue));
			}
			else
			{
				lua_pushstring(L, sValue.c_str());
			}
		
			lua_settable(L, 3);
		}

		SafeArrayUnaccessData(labelSA);
		SafeArrayDestroy(labelSA);
		SafeArrayUnaccessData(valueSA);
		SafeArrayDestroy(valueSA);
		return 1;
	}

	static void VariantToStack(lua_State* L, VARIANT* value)
	{
		switch (value->vt)
		{
		case VT_BSTR:

			lua_pushstring(L, _bstr_t(value->bstrVal));
			break;
		case VT_R4:
			lua_pushnumber(L, value->fltVal);
			break;
		case VT_R8:
			lua_pushnumber(L, value->dblVal);
			break;
		case VT_I4:
			lua_pushnumber(L, value->lVal);
			break;
		case VT_I2:
			lua_pushnumber(L, value->iVal);
			break;
		case VT_BOOL:
			lua_pushboolean(L, value->boolVal);
			break;
		case VT_UI1:
			lua_pushnumber(L, value->bVal);
			break;
		case VT_ERROR:
			lua_pushnumber(L, value->scode);
		case VT_EMPTY:
		case VT_NULL:
			lua_pushnil(L);
			break;
		default:
			luaL_error(L, "Unhandled VT type %d, tell Derek to support this. He'll know what to do :)", value->vt);
			break;
		}
		VariantClear(value);
	}	

	/***
	Create a new instance of the rawfile 

	@function 		New
	@string 		filePath The path to the rawfile
	@treturn 		rawFile The Lua wrapped access to the rawfile
	*/
	int newRawFile(lua_State* L)
	{
		const char* filePath = luaL_checkstring(L, -1);
		
		// Create the user data, just the size of a pointer to the struct
		RawFile **rawFile = reinterpret_cast<RawFile**>(lua_newuserdata(L, sizeof(RawFile*)));

		// Allocate the actual memory
		*rawFile = new RawFile(filePath);

		// Get the initialization error if present
		int result = (*rawFile)->init;

		if (result != 0)
		{
			luaL_error(L, "Error creating rawfile instance from COM: %d", result);
		}

		// Set the metatable for the userdata (on top of the stack) to the rawfile metatable
		luaL_getmetatable(L, RawFileType);
		lua_setmetatable(L, -2);

		lua_newtable(L);
		lua_pushstring(L, (*rawFile)->FileName);
		lua_setfield(L, -2, "FilePath");
		lua_setuservalue(L, -2);

		// return the userdata
		return 1;
	}
	
	/// a
	// @type rawFile


	int openRawFile(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);

		HRESULT hr = rawFile->comRawFile->Open(rawFile->FileName);
		if (FAILED(hr)) {
			lua_pushboolean(L, false);
			return 1;
		}
		hr = rawFile->comRawFile->SetCurrentController(0, 1); // MS device, 1st device
		rawFile->IsOpen = true;

		// Handle read once properties

		long firstScanNumber = 0;
		long lastScanNumber = 0;
		rawFile->comRawFile->GetFirstSpectrumNumber(&firstScanNumber);
		rawFile->comRawFile->GetLastSpectrumNumber(&lastScanNumber);

		// Get the user value for this userdata
		lua_getuservalue(L, 1);

		// Push fields
		lua_pushnumber(L, firstScanNumber);
		lua_setfield(L, -2, "FirstSpectrumNumber");

		lua_pushnumber(L, lastScanNumber);
		lua_setfield(L, -2, "LastSpectrumNumber");

		lua_pushboolean(L, rawFile->IsOpen);
		lua_setfield(L, -2, "IsOpen");

		lua_pushboolean(L, true);
		return 1;
	}
	
	int closeRawFile(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		rawFile->comRawFile->Close();
		rawFile->IsOpen = false;

		// Clear the user value table with a new one
		lua_newtable(L);
		lua_pushboolean(L, rawFile->IsOpen);
		lua_setfield(L, -2, "IsOpen");

		lua_setuservalue(L, 1);

		return 0;
	}

	int rawFileToString(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		lua_pushfstring(L, "RawFile: %s", rawFile->FileName);
		return 1;
	}

	int tuneData(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		long spectrumNumber = (long)luaL_checkinteger(L, 2) - 1;

		if (lua_gettop(L) == 3)
		{
			const char* key = luaL_checkstring(L, -1);

			VARIANT varValue;
			VariantInit(&varValue);
			HRESULT hr = rawFile->comRawFile->GetTuneDataValue(spectrumNumber, _bstr_t(key), &varValue);
			if (FAILED(hr))
			{
				return luaL_error(L, "Couldn't access key");
			}

			VariantToStack(L, &varValue);
			VariantClear(&varValue);

			return 1;
		}

		VARIANT labels;
		VARIANT values;
		VariantInit(&labels);
		VariantInit(&values);

		long size = 0;

		rawFile->comRawFile->GetTuneData(spectrumNumber, &labels, &values, &size);

		MapToStack(L, &labels, &values, size);
		return 1;
	}

	int scanTrailer(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		long spectrumNumber = (long)luaL_checkinteger(L, 2);

		if (lua_gettop(L) == 3)
		{
			const char* key = luaL_checkstring(L, 3);

			VARIANT varValue;
			VariantInit(&varValue);
			HRESULT hr = rawFile->comRawFile->GetTrailerExtraValueForScanNum(spectrumNumber, _bstr_t(key), &varValue);
			if (FAILED(hr))
			{
				return luaL_error(L, "Couldn't access key");
			}

			VariantToStack(L, &varValue);
			VariantClear(&varValue);

			return 1;
		}

		VARIANT labels;
		VARIANT values;
		VariantInit(&labels);
		VariantInit(&values);

		long size = 0;
		rawFile->comRawFile->GetTrailerExtraForScanNum(spectrumNumber, &labels, &values, &size);

		MapToStack(L, &labels, &values, size);
		return 1;
	}

	int statusLog(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		long spectrumNumber = (long)luaL_checkinteger(L, 2);

		if (lua_gettop(L) == 3)
		{
			const char* key = luaL_checkstring(L, 3);
			VARIANT varValue;
			VariantInit(&varValue);
			double dRt = 0;
			HRESULT hr = rawFile->comRawFile->GetStatusLogValueForScanNum(spectrumNumber, _bstr_t(key), &dRt, &varValue);
			if (FAILED(hr))
			{
				return luaL_error(L, "Couldn't access key");
			}

			VariantToStack(L, &varValue);
			VariantClear(&varValue);

			return 1;
		}

		VARIANT labels;
		VARIANT values;
		VariantInit(&labels);
		VariantInit(&values);

		long size = 0;
		double rt = 0;

		rawFile->comRawFile->GetStatusLogForScanNum(spectrumNumber, &rt, &labels, &values, &size);

		MapToStack(L, &labels, &values, size);
		return 1;
	}

	int scanFilter(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		long spectrumNumber = (long)luaL_checkinteger(L, 2);
		BSTR filter = NULL;
		rawFile->comRawFile->GetFilterForScanNum(spectrumNumber, &filter);
		lua_pushstring(L, _bstr_t(filter));
		SysFreeString(filter);
		return 1;
	}

	int scanHeader(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		long spectrumNumber = (long)luaL_checkinteger(L, 2);

		long nPackets = 0;
		double dStartTime = 0;
		double dLowMass = 0;
		double dHighMass = 0;
		double dTic = 0;
		double dBPMass = 0;
		double dBPIntensity = 0;
		long nChannels = 0;
		long nUniformTime = 0;
		double dFrequency = 0;

		rawFile->comRawFile->GetScanHeaderInfoForScanNum(spectrumNumber, &nPackets, &dStartTime, &dLowMass, &dHighMass,
			&dTic, &dBPMass, &dBPIntensity, &nChannels, &nUniformTime, &dFrequency);

		lua_createtable(L, 0, 10);
		luaD_setNumber(L, nPackets, "NumPackets");
		luaD_setNumber(L, dStartTime, "StartTime");
		luaD_setNumber(L, dLowMass, "LowMass");
		luaD_setNumber(L, dHighMass, "HighMass");
		luaD_setNumber(L, dTic, "TIC");
		luaD_setNumber(L, dBPMass, "BasePeakMass");
		luaD_setNumber(L, dBPIntensity, "BasePeakIntensity");
		luaD_setNumber(L, nChannels, "NumChannels");
		luaD_setNumber(L, nUniformTime, "UniformTime");
		luaD_setNumber(L, dFrequency, "Frequency");

		return 1;
	}

	int getNumInstMethods(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		try {	
			long number(0);			
			HRESULT hr = rawFile->comRawFile->GetNumInstMethods(&number);
			if (SUCCEEDED(hr)) {
				lua_pushnumber(L, number);
				return 1;
			}
		}
		catch (...) {}
		lua_pushnil(L);
		return 1;
	}

	int getInstrumentMethod(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		try {
			int index = (int)luaL_checkinteger(L, -1) - 1;

			BSTR str = NULL;
			HRESULT hr = rawFile->comRawFile->GetInstMethod(index, &str);
			if (SUCCEEDED(hr) && str != NULL) {
				CStringA returnString = CStringA(str);
				SysFreeString(str);
				lua_pushstring(L, returnString);				
				return 1;
			}
		}
		catch (...) {}
		lua_pushnil(L);
		return 1;
	}

	int getInstrumentMethodNames(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);	
				
		try {		
			long number(0);
			VARIANT names;
			VariantInit(&names);
			HRESULT hr = rawFile->comRawFile->GetInstMethodNames(&number, &names);
			if (SUCCEEDED(hr)) 
			{
				ListToTable(L, &names, number);
				VariantClear(&names);
				lua_pushnumber(L, number);
				return 2;
			}
			VariantClear(&names);
		}
		catch (...) {}	
		lua_pushnil(L);
		return 1;
	}
	
	int getIsolationWidthForScanNum(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		long sn = (long)luaL_checkinteger(L, 2);
		long msOrder = 2;

		if (lua_gettop(L) == 3) 
		{
			msOrder = (long)luaL_checkinteger(L, 3);
		}

		double width = 0;
		rawFile->comRawFile->GetIsolationWidthForScanNum(sn, msOrder, &width);

		lua_pushnumber(L, width);
		lua_pushinteger(L, msOrder);
		return 2;
	}

	int getRetentionTime(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		long spectrumNumber = (long)luaL_checkinteger(L, 2);
		double dRT = 0;
		rawFile->comRawFile->RTFromScanNum(spectrumNumber, &dRT);
		lua_pushnumber(L, dRT);
		return 1;
	}

	int getScanNumberFromRT(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		double dRT = luaL_checknumber(L, 2);
		long spectrumNumber;
		rawFile->comRawFile->ScanNumFromRT(dRT, &spectrumNumber);
		rawFile->comRawFile->RTFromScanNum(spectrumNumber, &dRT);
		lua_pushinteger(L, spectrumNumber);
		lua_pushnumber(L, dRT);
		return 2;
	}

	int getMSNOrder(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		long spectrumNumber = (long)luaL_checkinteger(L, 2);
		long msnOrder = -1;
		rawFile->comRawFile->GetMSOrderForScanNum(spectrumNumber, &msnOrder);
		lua_pushinteger(L, msnOrder);
		return 1;
	}

	int hasCentroidData(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		long spectrumNumber = (long)luaL_checkinteger(L, 2);
		long centroid = 0;
		rawFile->comRawFile->IsCentroidScanForScanNum(spectrumNumber, &centroid);
		lua_pushboolean(L, centroid);
		return 1;
	}

	int getChroData(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);

		if (!lua_istable(L, 2)) {
			luaL_argerror(L, -2, "Expecting Table of parameters for Chro Data");
			return 0;
		}

		lua_getfield(L, -1, "Type");
		long chroType = (long)lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "StartTime");
		double startTime = 0;
		if (lua_isnumber(L, -1))
			startTime = lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "EndTime");
		double endTime = 0;
		if (lua_isnumber(L, -1))
			endTime = lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "Delay");
		double delay = 0;
		if (lua_isnumber(L, -1))
			delay = lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "Operator");
		long chroOperator = 0;
		if (lua_isnumber(L, -1))
			chroOperator = (long)lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "Type2");
		long chroType2 = 0;
		if (lua_isnumber(L, -1))
			chroType2 = (long)lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "Filter");
		_bstr_t filter = "";
		if (lua_isstring(L, -1))
			filter = lua_tostring(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "MassRange1");
		_bstr_t massRange1 = "";
		if (lua_isstring(L, -1))
			massRange1 = lua_tostring(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "MassRange2");
		_bstr_t massRange2 = "";
		if (lua_isstring(L, -1))
			massRange2 = lua_tostring(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "SmoothingType");
		long smoothingType = 0;
		if (lua_isnumber(L, -1))
			smoothingType = (long)lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "SmoothingValue");
		long smoothingValue = 3;
		if (lua_isnumber(L, -1))
			smoothingValue = (long)lua_tonumber(L, -1);
		lua_pop(L, 1);

		long size = 0;
		VARIANT chroData;
		VARIANT flags;
		VariantInit(&chroData);
		VariantInit(&flags);

		HRESULT hr = rawFile->comRawFile->GetChroData(chroType, chroOperator, chroType2, filter, massRange1, massRange2, delay, &startTime, &endTime, smoothingType, smoothingValue, &chroData, &flags, &size);

		lua_createtable(L, size, 2);
		luaD_setNumber(L, startTime, "StartTime");
		luaD_setNumber(L, endTime, "EndTime");

		ChroPeak* pValues = NULL;
		SAFEARRAY FAR* valueSA = chroData.parray;
		SafeArrayAccessData(valueSA, (void**)(&pValues));

		for (int i = 0; i < size; i++)
		{
			lua_createtable(L, 0, 2);
			luaD_setNumber(L, pValues[i].dTime, "Time");
			luaD_setNumber(L, pValues[i].dIntensity, "Intensity");

			lua_rawseti(L, -2, i + 1);
		}

		SafeArrayUnaccessData(valueSA);
		SafeArrayDestroy(valueSA);

		return 1;
	}

	int getSpectrumData(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		long spectrumNumber = (long)luaL_checkinteger(L, 2);

		double fm = 0;
		double lm = 1000000000;

		if (lua_gettop(L) > 2) {
			luaL_checktype(L, 3, LUA_TTABLE);
			lua_getfield(L, 3, "fm");
			if (lua_isnumber(L, -1))
			{
				fm = lua_tonumber(L, -1);
			}
			lua_getfield(L, 3, "lm");
			if (lua_isnumber(L, -1))
			{
				lm = lua_tonumber(L, -1);
			}
			lua_pop(L, 2);
		}

		VARIANT massList;
		VariantInit(&massList);
		VARIANT peakFlags;
		VariantInit(&peakFlags);
		long size;
		double centroidPeakWidth = 0;	
		rawFile->comRawFile->GetMassListFromScanNum(&spectrumNumber, (LPCTSTR)NULL, 0, 0, 0, 0, &centroidPeakWidth, &massList, &peakFlags, &size);
				
		SAFEARRAY FAR* psa = massList.parray;
		DataPeak* pDataPeaks = NULL;
		SafeArrayAccessData(psa, (void**)(&pDataPeaks));	
	
		lua_createtable(L, size, 0);
		int c = 1;
		for (int i = 0; i < size; i++)
		{
			double mass = pDataPeaks[i].Mass;
			if (mass < fm || mass > lm)
				continue;

			lua_createtable(L, 0, 2);
			luaD_setNumber(L, mass, "Mass");
			luaD_setNumber(L, pDataPeaks[i].Intensity, "Intensity");
			lua_rawseti(L, -2, c++);
		}	
				
		SafeArrayUnaccessData(psa);		
		SafeArrayDestroy(psa);	
		
		VariantClear(&peakFlags);

		return 1;
	}

	int getLabelData(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		long spectrumNumber = (long)luaL_checkinteger(L, 2);

		double fm = 0;
		double lm = 1000000000;

		if (lua_gettop(L) > 2) {
			luaL_checktype(L, 3, LUA_TTABLE);
			lua_getfield(L, 3, "fm");
			if (lua_isnumber(L, -1))
			{
				fm = lua_tonumber(L, -1);
			}
			lua_getfield(L, 3, "lm");
			if (lua_isnumber(L, -1))
			{
				lm = lua_tonumber(L, -1);
			}
			lua_pop(L, 2);
		}

		VARIANT labels;
		VARIANT flags;
		VariantInit(&labels);
		VariantInit(&flags);

		rawFile->comRawFile->GetLabelData(&labels, &flags, &spectrumNumber);

		LabelData* pValues = NULL;
		SAFEARRAY FAR* valueSA = labels.parray;
		SafeArrayAccessData(valueSA, (void**)(&pValues));

		LabelFlags* pFlags = NULL;
		SAFEARRAY FAR* flagsSA = flags.parray;
		SafeArrayAccessData(flagsSA, (void**)(&pFlags));

		int size = labels.parray->rgsabound[0].cElements;
		lua_createtable(L, size, 0);
		int c = 1;
		for (int i = 0; i < size; i++)
		{
			double mass = pValues[i].Mass;
			if (mass < fm || mass > lm)
				continue;

			lua_createtable(L, 0, 6);
			luaD_setNumber(L, mass, "Mass");
			luaD_setNumber(L, pValues[i].Intensity, "Intensity");
			luaD_setNumber(L, pValues[i].Baseline, "Baseline");
			luaD_setNumber(L, pValues[i].Noise, "Noise");
			luaD_setNumber(L, pValues[i].Resolution, "Resolution");
			luaD_setNumber(L, pValues[i].Charge, "Charge");

			if (pFlags[i].Exception)
			{
				lua_pushboolean(L, true);
				lua_setfield(L, -2, "Exception");
			}

			if (pFlags[i].Fragmented)
			{
				lua_pushboolean(L, true);
				lua_setfield(L, -2, "Fragmented");
			}

			if (pFlags[i].Merged)
			{
				lua_pushboolean(L, true);
				lua_setfield(L, -2, "Merged");
			}

			if (pFlags[i].Modified)
			{
				lua_pushboolean(L, true);
				lua_setfield(L, -2, "Modified");
			}

			if (pFlags[i].Saturated)
			{
				lua_pushboolean(L, true);
				lua_setfield(L, -2, "Saturated");
			}

			lua_rawseti(L, -2, c++);
		}

		SafeArrayUnaccessData(valueSA);
		SafeArrayDestroy(valueSA);
		SafeArrayUnaccessData(flagsSA);
		SafeArrayDestroy(flagsSA);


		return 1;
	}

	/***
	Get the precursor mass of a given MSn stage in a spectrum
	@function GetPrecursorMass
	@int 			sn The spectrum number
	@int[opt=2] 	msn The precursor MSn stage
	@treturn 		float The precursor mass at the given stage
	*/
	int getPrecursorMass(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		long sn = (long)luaL_checkinteger(L, 2);
		long msOrder = 2;

		if (lua_gettop(L) == 3) {
			 msOrder = (long)luaL_checkinteger(L, 3);
		}
	
		double mass = 0;
		int charge = 0;
		rawFile->comRawFile->GetPrecursorMassForScanNum(sn, msOrder, &mass);	

		lua_pushnumber(L, mass);
		lua_pushinteger(L, msOrder);
		return 2;
	}

	int getInAcquisition(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		long inAcq = 0;
		rawFile->comRawFile->InAcquisition(&inAcq);		
		lua_pushboolean(L, inAcq);
		return 1;
	}

	int getSegmentsForScanNumber(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		long sn = (long)luaL_checkinteger(L, 2);
		long segs = 0;
		long events = 0;
		rawFile->comRawFile->GetSegmentAndEventForScanNum(sn, &segs, &events);
		lua_pushinteger(L, segs);
		lua_pushinteger(L, events);
		return 2;
	}

	int getLowMass(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		double lowMass(0);
		HRESULT hr = rawFile->comRawFile->GetLowMass(&lowMass);
		if (FAILED(hr)) 
		{
			lua_pushboolean(L, false);
			return 1;
		}
		lua_pushnumber(L, lowMass);
		return 1;
	}

	int getHighMass(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		double highMass(0);
		HRESULT hr = rawFile->comRawFile->GetHighMass(&highMass);
		if (FAILED(hr))
		{
			lua_pushboolean(L, false);
			return 1;
		}
		lua_pushnumber(L, highMass);
		return 1;
	}

	int getInstName(lua_State* L)
	{	
		RawFile *rawFile = checkRawFile(L);
		BSTR name = NULL; // the assignment to null is needed for some odd reason
		HRESULT hr = rawFile->comRawFile->GetInstName(&name);
		if (FAILED(hr))
		{
			lua_pushboolean(L, false);
			return 1;
		}
		lua_pushstring(L, _bstr_t(name));
		return 1;
	}

	int getSWVersion(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		BSTR version = NULL; // the assignment to null is needed for some odd reason
		HRESULT hr = rawFile->comRawFile->GetInstSoftwareVersion(&version);
		if (FAILED(hr))
		{
			lua_pushboolean(L, false);
			return 1;
		}
		lua_pushstring(L, _bstr_t(version));
		return 1;
	}

	int getSerialNumber(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		BSTR serialNumber = NULL;
		HRESULT hr = rawFile->comRawFile->GetInstSerialNumber(&serialNumber);
		if (FAILED(hr))
		{
			lua_pushboolean(L, false);
			return 1;
		}
		lua_pushstring(L, _bstr_t(serialNumber));
		return 1;
	}

	int getHWVersion(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		BSTR version = NULL;
		HRESULT hr = rawFile->comRawFile->GetInstHardwareVersion(&version);
		if (FAILED(hr))
		{
			lua_pushboolean(L, false);
			return 1;
		}
		lua_pushstring(L, _bstr_t(version));
		return 1;
	}

	int getInstModel(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		BSTR model = NULL;
		HRESULT hr = rawFile->comRawFile->GetInstModel(&model);
		if (FAILED(hr))
		{
			lua_pushboolean(L, false);
			return 1;
		}
		lua_pushstring(L, _bstr_t(model));
		return 1;
	}
	


	int getErrorLogCount(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);		
		long count(0);
		HRESULT hr = rawFile->comRawFile->GetNumErrorLog(&count);
		if (FAILED(hr))
		{
			lua_pushboolean(L, false);
			return 1;
		}
		lua_pushinteger(L, count);
		return 1;
	}

	int getErrorLogItem(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		long id = (long)luaL_checkinteger(L, 2);
		BSTR message = NULL; // the assignment to null is needed for some odd reason
		double rt(0);
		HRESULT hr = rawFile->comRawFile->GetErrorLogItem(id, &rt, &message);
		if (FAILED(hr))
		{
			lua_pushboolean(L, false);
			return 1;
		}		
		lua_pushstring(L, _bstr_t(message));
		lua_pushnumber(L, rt);
		return 2;
	}

	int releaseRawfile(lua_State* L)
	{
		RawFile *rawFile = checkRawFile(L);
		delete rawFile;
		return 0;
	}

	int Register(lua_State* L)
	{
		luaL_newmetatable(L, RawFileType);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
		luaL_setfuncs(L, thermo_rawfile_m, 0);
		return 1;
	}

}
