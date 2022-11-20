local zmq = require("lzmq")

os.execute("sleep 1")




function GetData_FromZmqSocket()
    local ctx = zmq.context()
    local skt = ctx:socket{ zmq.REQ, linger=0, rcvtimeo=1500, connect = "ipc:///var/tmp/TemperCommandChannel" }

    if skt == nil then
        ctx:destroy()
        return nil
    end

    skt:send( "ok" )

    local Adata = skt:recv()

    skt:close()
    ctx:destroy()

    return Adata
end



local S1,i,j,I,W,tmpx,tmpy,T

local SS = GetData_FromZmqSocket()


if SS == nil then

    S1 = '{ "TemperGet": ["error","skt:recv() returned nil"] } '

else
        W={}
        i,j = string.find(SS,",")
        I   = 1

        while i ~= nil do
            W[#W+1] = string.sub(SS,I,i-1)
            I = i+1
            i,j = string.find(SS,",",i+1)
        end

        W[#W+1] = string.sub(SS,I,#SS-1)

     -- tmpx = tonumber(W[4]) - 3.0
        tmpx = tonumber(W[4])

        if W[23] == nil then
            tmpy = 0.8
        else
            tmpy = (tonumber(W[23]) * 1.8) + 32
        end

        if W[15] == nil then
            tmpz = 0.9
        else
            tmpz = (tonumber(W[15]) * 1.8) + 32                      -- just before SOC
        end

        if W[5] == "<nosolar>" then
            W[5]  = "0.1"
            W[6]  = "0.2"
            W[7]  = "0.3"
            W[11] = "0.4"
            W[16] = "0.5"
            W[17] = "0.6"
            W[18] = "0.7"
            W[19] = "x"
            W[20] = "x"
            W[21] = "x"
        end
        

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
end

S1 = string.format('{ "TemperData": [ %s ] }', table.concat(T,","))

file=io.open("/tmp/t1.json","w");
file:write(S1)
file:close()



os.execute("/usr/bin/curl -F \"the_file=@/tmp/t1.json\" http://hh2.loogatee.com/RSSpageSave?Feedname=E\\&League=HH_2029");




