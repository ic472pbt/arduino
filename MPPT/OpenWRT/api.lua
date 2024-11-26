#!/usr/bin/env lua
local sqlite3 = require("luasql.sqlite3")
local lfs = require("lfs")

-- Enable debugging
lfs.debug = true

local env = sqlite3.sqlite3()
local db = env:connect("/mnt//sdb1/solar/sensor_data.db")

-- Helper function: Parse HTTP query parameters
local function parse_query(qs)
    local params = {}
    for k, v in string.gmatch(qs, "([^&=]+)=([^&=]+)") do
        params[k] = v
    end
    return params
end

-- Handle GET request
local function handle_get(params)
    local query = "SELECT * FROM sensor_data"
    if params["id"] then
        query = query .. " WHERE id = " .. params["id"]
    elseif params["from"] then
        local fromn = string.sub(params["from"], 1, -4)
        local ton = string.sub(params["to"], 1, -4)
    -- os.execute("logger '" .. fromn .. "'")
        query = query .. string.format(" WHERE id between %s AND %s LIMIT 5000", fromn, ton)
    else
        query = query .. " ORDER BY id DESC LIMIT 1"
    end

    local result = {}
    local cursor, err = db:execute(query)
    if not cursor then
        return { error = "Failed to execute query: " .. (err or "unknown error") }
    end

    local row = cursor:fetch({}, "a")
    while row do
        table.insert(result, row)
        row = cursor:fetch({}, "a")
    end
    cursor:close()
    return result
end

-- Send JSON response
local function send_json(data)
    print("Content-Type: application/json\n")
    local json = require("dkjson")
    local json_data = json.encode(data, { indent = true })
    print(json_data)
end

-- Main function
local function main()
    local method = os.getenv("REQUEST_METHOD") or "GET"
    local query_string = os.getenv("QUERY_STRING") or "from=2000&to=3000"

    if method == "GET" then
        local params = parse_query(query_string)
        local data = handle_get(params)
        send_json(data)
    else
        send_json({ error = "Unsupported HTTP method: " .. method })
    end
end

main()


