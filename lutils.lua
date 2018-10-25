
--package.path = package.path .. ';/home/johnr/CDCsim/?.lua'
require 'string'

function find_hidraw()
    local p1,p2,Adir,retv,D
    local T = {}

    retv = ""

    p1 = io.popen('ls -1 /sys/bus/usb/devices')
    for D in p1:lines() do

         Adir = {}
         p2 = io.popen('find /sys/bus/usb/devices/' .. D .. '/* -name \\*413D:2107\\* -print')
         for V in p2:lines() do
             Adir[#Adir+1] = V
         end
         if #Adir == 2 then break end             -- 1st one with 2 hits: Done!

    end

    if #Adir ~= 2 then
        T[#T+1] = retv
    else
        p2 = io.popen( 'ls -1 ' .. Adir[2] .. '/hidraw' )
        for D in p2:lines() do
            retv = D
            break
        end
        T[#T+1] = '/dev/' .. retv
    end

    return T
end


--Val = find_hidraw()
--if Val == "" then
--    print("Temper Device Not Found")
--else
--    print("Temper Device: " .. Val)
--end




