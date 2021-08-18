--
-- Copyright 2019 John Reed
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not  use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
-- http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations  under the License.
--

local lf  = require("luafcgid")
local zmq = require("lzmq")

--
--  TemperGet
--
function main(env, con)
    local SS,S1

    local ctx = zmq.context()
    local skt = ctx:socket{ zmq.REQ, linger=0, rcvtimeo=1500, connect = "ipc:///var/tmp/TemperCommandChannel" }

    if skt == nil then
        ctx:destroy()
        return
    end

    skt:send( "ok" )

    SS = skt:recv()

    if SS == nil then
        S1 = '{ "TemperGet": ["error","skt:recv() returned nil"] } '
    else

        local i,j,I,W,tmpx,tmpy,T

        W={}
        i,j = string.find(SS,",")
        I   = 1

        while i ~= nil do
            W[#W+1] = string.sub(SS,I,i-1)
            I = i+1
            i,j = string.find(SS,",",i+1)
        end

        W[#W+1] = string.sub(SS,I,#SS-1)

        tmpx = tonumber(W[4]) - 3.0
        tmpy = (tonumber(W[23]) * 1.8) + 32
        tmpz = (tonumber(W[15]) * 1.8) + 32                      -- just before SOC

        T = {}
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
        T[#T+1] = '"aa"'                                         -- 
        T[#T+1] = '"bb"'                                         -- 
        T[#T+1] = '"cc"'                                         -- 
        T[#T+1] = '"dd"'                                         -- 

        S1 = string.format('{ "TemperData": [ %s ] }', table.concat(T,","))
    end

    skt:close()
    ctx:destroy()

    con:header("Content-Type", "application/json")
    con:header("Connection",   "close")
    con:puts(S1)
end
