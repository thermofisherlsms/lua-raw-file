# lua-raw-file

## About
Lua wrapper to access data from the Thermo raw data file format

The module is compatible with Lua 5.1, 5.2, 5.3 and Lua JIT (5.1).

We recommend the [ZeroBrane Studio](https://github.com/pkulchenko/ZeroBraneStudio/) IDE for editing and executing Lua scripts.

This repository is intended to be used for scholarly research, and is therefore made available as-is.  Ongoing maintenance and support is not generally available; however, issues and improvements will be considered on a case-by-case basis.

# Usage

## Example of accessing MS peaks from a raw file

```lua

-- Load the module
local RawFile = require("LuaRawFile")	

-- Open a new raw file
local rawFile = RawFile.New("BasicRawFile.raw")

-- Open the connection to the raw file
rawFile:Open()

-- Get the 10th spectrum
local spectrum = rawFile:GetSpectrum(10)

-- Iterate over all the peaks and print out the m/z and intensity
for i, peak in ipairs(spectrum) do
	print(i, peak.Mass, peak.Intensity)
end

-- Close the connection to the raw file
rawFile:Close()

```

See the [Example Program](example/RawFileExample.lua) for complete details.


# Requirements

 * MS File Reader (3.0.31 SP2 or newer)
 * XCalibur 3.1 (optional if MS File Reader is installed)
 
 This Lua module uses COM to interact with either the MS File Reader or XCalibur XRawfile2 component to provide access to the .raw file format.
 
# License
 
This software may be modified and distributed under the terms of the MIT license.  See the [LICENSE](LICENSE.md) file for details.