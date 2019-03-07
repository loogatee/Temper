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
