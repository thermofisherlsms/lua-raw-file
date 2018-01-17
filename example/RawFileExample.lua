--[[ 
 RawFileExample.lua
 
 Copyright (C) 2016 Thermo Fisher Scientific
 
 This software may be modified and distributed under the terms
 of the MIT license.  See the LICENSE file for details.
--]]

-- ****************************************************************
-- ** Change the lookup path to the library
-- ** In your app you need to either point to the right library or
-- ** have it already defined in your package.path/cpath directory
do
local v = _VERSION:gsub("Lua ", ""):gsub("%.","")
package.path = string.format("../bin/%s/?.lua;%s",v,package.path)
package.cpath = string.format("../bin/%s/?.dll;%s",v,package.cpath)
end
-- ****************************************************************

-- Load the module
local RawFile = require("LuaRawFile")			

print(RawFile.Version)	

-- Create a rawfile given a path to a rawfile
local rawFile = assert(RawFile.New("Basic.raw"))

print(type(rawFile))			-- RawFile is userdata in c++
print(rawFile.FilePath)			-- Filepath to the raw file
print(rawFile:Open())			-- Open the connection to read data
print(rawFile.IsOpen)			-- Status if the connection is open or not
print(rawFile:InAcquisition())	-- Usually will be false, variable to spin on
print(rawFile:GetInstrumentMethod(1))

print(rawFile:GetLowMass())
print(rawFile:GetHighMass())
print(rawFile:GetInstName())
print(rawFile:GetInstSoftwareVersion())
print(rawFile:GetNumErrorLog())
print(rawFile:GetErrorLogItem(1))

-- Helper function 
local printf = function(formatString, ...)
	print(string.format(formatString, ...))
end

print(rawFile.FirstSpectrumNumber, rawFile.LastSpectrumNumber, #rawFile)

-- Print all the Retention Times for this rawfile
for sn = rawFile.FirstSpectrumNumber, rawFile.LastSpectrumNumber do
	printf("The retention time for scan %d is %f",sn,rawFile:GetRetentionTime(sn))
end

local spectrum = rawFile:GetSpectrum(10)
--print(spectrum)
--print (#spectrum)
--local allPeaks=spectrum:GetPeaks()
--print(#allPeaks)
for i, peak in ipairs(spectrum) do
	print(i, peak.Mass, peak.Intensity)
end

print(rawFile:GetScanTrailer(10, "Ion Injection Time (ms):"))
print(rawFile:GetScanTrailer(10, "AGC:"))
print(rawFile:GetScanTrailer(10, "Micro Scan Count:"))
print(rawFile:GetScanTrailer(10, "Elapsed Scan Time (sec):"))

print(rawFile:GetScanFilter(10))
print(rawFile:GetScanNumberFromRT(0.025)) -- returns the SN and actual RT
print(rawFile:GetMSNOrder(10))
print(rawFile:HasCentroidData(10))

print("== Header ==")
local header = rawFile:GetScanHeader(10)
for k,v in pairs(header) do
	print(k,v)
end

-- The base getTrailerExtra function returns everything as a string
-- Call the GetTrailerExtraValue to have Lua convert it to the approriate type
print("== Trailer ==")
local trailer = rawFile:GetScanTrailer(10)
for k,v in pairs(trailer) do
	local realvalue = rawFile:GetScanTrailer(10, k)
	print(k,v, realvalue, type(realvalue))	
end

print("== Status Log ==")
local statusLog = rawFile:GetStatusLog(10)
for k,v in pairs(statusLog) do
	local realvalue = rawFile:GetStatusLog(10, k)
	print(k,v, realvalue, type(realvalue))	
end

print("== Tune Data ==")
local tuneData = rawFile:GetTuneData(1)
for k,v in pairs(tuneData) do
	local realvalue = rawFile:GetTuneData(1, k)
	print(k,v, realvalue, type(realvalue))	
end

print("== Chro Data ==")
local chro = rawFile:GetChroData({ 	-- see MsFileReader doc for complete details
	Type = 				1,			-- 0 Mass Range, 1 TIC, Base Peak 2
	Operator = 			0,			-- 0 None, 1 Minus, 2 Plus
	Type2 = 			0,			-- 0 Mass Range, 1 Base Peak
	Filter = 			nil,		-- Scan Filter
	MassRange1 = 		nil,		-- Mass Range for chro1
	MassRange2 = 		nil,		-- Mass Range for chro2
	SmoothingType = 	0,			-- 0 None, 1 Boxcar, 2 Gaussian
	SmoothingValue = 	3,			-- Odd value between 3-15
	Delay = 			0,
	StartTime = 		0,			-- 
	EndTime = 			0,			--
	})

print(chro.StartTime,chro.EndTime)
for _,chroPoint in ipairs(chro) do	
	print(chroPoint.Time,chroPoint.Intensity)	
end

print("== Label Data ==")
local peaks = rawFile:GetLabelData(1)
for _,labelPeak in ipairs(peaks) do	
	print(labelPeak.Mass,labelPeak.Intensity, labelPeak.Noise, labelPeak.Baseline, labelPeak.Resolution, labelPeak.Charge, labelPeak.Exception,labelPeak.Fragmented, labelPeak.Merged, labelPeak.Modified, labelPeak.Saturated)	
end

print("== Precursor Data==")
print(rawFile:GetPrecursorMass(13))

