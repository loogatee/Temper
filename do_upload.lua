local zmq = require("lzmq")

os.execute("sleep 1")


--
--   Gets Temperature and Battery Charger Data.
--
--      Temperature data comes from the 'Temper' USB device
--      Battery Charger Data comes from the Epever Solar Charger/Controller in the Van
--
--
function GetData_FromZmqSocket()
    local ctx = zmq.context()
    local skt = ctx:socket{ zmq.REQ, linger=0, rcvtimeo=1500, connect = "ipc:///var/tmp/TemperCommandChannel" }

    if skt == nil then
        ctx:destroy()
        return nil
    end

    skt:send( "ok" )

    local TBdata = skt:recv()

    skt:close()
    ctx:destroy()

    return TBdata     -- Temperature and Battery Data
end




--
--    "52727,11/20/2022,14:38:47,79.2,<nosolar>\n"
--
-- [^,\n]+    Matches all characters except for ',' and '\n'
--                The '+' is "1 or more repititions"
-- [,?$?]-    Single character to terminate: Either , or EOL
--                The '-' is "0 or more repititions"
--
--    W[1]: 52727
--    W[2]: 11/20/2022
--    W[3]: 14:38:47
--    W[4]: 79.2
--    W[5]: <nosolar>
--   #W[5]: 9
function Convert_ToJSON(Istr)
    local rjson  = '{ "TemperGet": ["error","skt:recv() returned nil"] } '

    if Istr ~= nil then

        local W = {}
        for y in string.gmatch(Istr,"[^,\n]+[,?$?]-") do
           W[#W+1] = y
        end

        local tmpx = tonumber(W[4])
        local tmpy = 0.8;            if W[23] ~= nil then tmpy = (tonumber(W[23]) * 1.8) + 32 end
        local tmpz = 0.9;            if W[15] ~= nil then tmpz = (tonumber(W[15]) * 1.8) + 32 end

        if W[5] == "<nosolar>" then
            W[5]  = "0.1";  W[6]  = "0.2";  W[7]  = "0.3"; W[11] = "0.4";  W[16] = "0.5";
            W[17] = "0.6";  W[18] = "0.7";  W[19] = "x";   W[20] = "x";    W[21] = "x";
        end

        local T = {}
        T[#T+1] = string.format('"%s %s"',W[2],W[3])             -- date time
        T[#T+1] = string.format('"%.1f"',tmpx)                   -- temperature from temper
        T[#T+1] = string.format('"%s"',W[5])                     -- volts    from panel
        T[#T+1] = string.format('"%s"',W[6])                     -- current  from panel
        T[#T+1] = string.format('"%s"',W[7])                     -- watts    from panel
        T[#T+1] = string.format('"%s"',W[11])                    -- Voltage of batteries
        T[#T+1] = string.format('"%s"',W[16])                    -- SOC
        T[#T+1] = string.format('"%s"',W[17])                    -- battery status
        T[#T+1] = string.format('"%s"',W[18])                    -- charging equip status
        T[#T+1] = string.format('"%s/%s/%s"',W[19],W[20],W[21])  -- Generated Energy Today/Monthly/Yearly
        T[#T+1] = string.format('"%.1f"',tmpy)                   -- temperature from the battery probe
        T[#T+1] = string.format('"%.1f"',tmpz)                   -- temperature inside the Charger
        T[#T+1] = '"aa","bb","cc","dd"'                          -- filler

        rjson = string.format('{ "TemperData": [ %s ] }', table.concat(T,","))
    end

    return rjson
end


local S1 = GetData_FromZmqSocket()
local S2 = Convert_ToJSON(S1)

local file=io.open("/tmp/t1.json","w");

file:write(S2)
file:close()

os.execute("/usr/bin/curl -F \"the_file=@/tmp/t1.json\" http://hh2.loogatee.com/RSSpageSave?Feedname=E\\&League=HH_2029");

