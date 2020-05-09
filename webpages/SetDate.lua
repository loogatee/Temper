-- utilies for processing a FCGI request
local lf = require("luafcgid")

function main(env, con)
    local thedate = ""
    local s = {}
    local params = lf.parse(env.QUERY_STRING)

    for n,v in pairs(params) do
        if n == "D" then
            thedate = v
            break
        end
    end

    os.execute("date " .. thedate)

    con:puts("ok\n\r")
end
