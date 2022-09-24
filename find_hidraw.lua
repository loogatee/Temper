--**
--* Copyright 2018 John Reed
--*
--* Licensed under the Apache License, Version 2.0 (the "License");
--* you may not  use this file except in compliance with the License.
--* You may obtain a copy of the License at
--*
--* http://www.apache.org/licenses/LICENSE-2.0
--*
--* Unless required by applicable law or agreed to in writing, software
--* distributed under the License is distributed on an "AS IS" BASIS,
--* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
--* See the License for the specific language governing permissions and
--* limitations  under the License.
--**

--package.path = package.path .. ';/home/johnr/CDCsim/?.lua'


require 'string'

function find_hidraw()
    local p1,p2,Adir,retv,D

    retv = ""

    p1 = io.popen('ls -1 /sys/bus/usb/devices')
    for D in p1:lines() do

         Adir = {}
       --p2 = io.popen('find /sys/bus/usb/devices/' .. D .. '/* -name \\*413D:2107\\* -print')
         p2 = io.popen('find /sys/bus/usb/devices/' .. D .. '/* -name \\*1A86:E025\\* -print')
         for V in p2:lines() do
             print(D .. "    " .. V)
             Adir[#Adir+1] = V
         end

         print("======")

         if #Adir == 2 then break end     -- 1st one with 2 hits: Done!

    end

    if #Adir ~= 2 then return retv end

    p2 = io.popen( 'ls -1 ' .. Adir[2] .. '/hidraw' )
    for D in p2:lines() do
        retv = D
        break
    end

    return '/dev/' .. retv
end


Val = find_hidraw()
if Val == "" then
    print("Temper Device Not Found")
else
    print("Temper Device: " .. Val)
end




