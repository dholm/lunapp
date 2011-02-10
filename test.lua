--[[
 Licensed under the MIT license:
  http://www.opensource.org/licenses/mit-license.php
--]]

print("begin")
test = Test(1)
print("Test=" .. test:getValue())
print("Instance=" .. test:getInstance():getValue())
print("end")
