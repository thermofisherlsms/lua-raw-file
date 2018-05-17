local M = {}

local mt = {}

require("luanet")
require("CLRPackage")

luanet.load_assembly("ThermoFisher.CommonCore.Data")
luanet.load_assembly("ThermoFisher.CommonCore.RawFileReader")

local RFRA = luanet.import_type("ThermoFisher.CommonCore.RawFileReader.RawFileReaderAdapter")
local device = luanet.import_type("ThermoFisher.CommonCore.Data.Business.Device")

local function enum(o)
   local e = o:GetEnumerator()
   return function()
      if e:MoveNext() then
        return e.Current
     end
   end
end

function mt:Open()
    local rf = self.rawfile
    
    rf:SelectInstrument(device.MS, 1)
    local runheader = rf.RunHeaderEx
    
    self.IsOpen = true
    self.FirstSpectrumNumber = runheader.FirstSpectrum
    self.LastSpectrumNumber = runheader.LastSpectrum
    
    return true
end

function mt:GetSpectrum(scanNumber)
    local simplifiedCentroids = self.rawfile:GetSimplifiedCentroids(scanNumber)
    local spectrum = {}
    local i = 0
--    for mass in enum(simplifiedCentroids.Masses) do
--        spectrum[#spectrum + 1] = {
--           Mass = mass,
--           Intensity = simplifiedCentroids.Intensities[i]
--        }       
--        i = i + 1
--    end
    return simplifiedCentroids
end

-- Module functions

function M.New(filename)
    local o = {FilePath = filename}
    setmetatable(o, {__index = mt})
    o.rawfile = RFRA.FileFactory(filename)
    return o
end

function M.GetRawFileMetaTable()
   return mt 
end

return M