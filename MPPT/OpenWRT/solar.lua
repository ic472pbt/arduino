local config = require("config")
local socket = require("socket") -- LuaSocket for TCP communication
local luasql = require("luasql.sqlite3") -- LuaSQL for SQLite
local http = require("socket.http") -- LuaSocket for HTTP requests
local json = require("dkjson") -- dkjson for JSON encoding

-- Database configuration
local db_file = "/mnt/sdb1/solar/sensor_data.db"
local env = luasql.sqlite3()
local conn = assert(env:connect(db_file))

-- Create a table for storing sensor data
local create_table_query = [[
CREATE TABLE IF NOT EXISTS sensor_data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    time TEXT,
    current_panel REAL,
    voltage_panel REAL,
    power_panel REAL,
    battery_voltage REAL,
    battery_current REAL,
    load_current REAL,
    charging_status TEXT,
    pwm REAL,
    out_temp REAL,
    sys_temp REAL,
    pv_float REAL
);
]]
assert(conn:execute(create_table_query))

-- Server configuration
local host = "192.168.1.4"
local port = 8088

-- Create and connect the socket
local client = assert(socket.tcp())
client:connect(host, port)
client:send("i")

-- Receive data from the server
local values = ""
local txt = {}
while true do
    local data, err = client:receive("*l")
    if not data or err then break end
    table.insert(txt, data)
    if data:find("#") then break end
end
client:close()

-- Get current timestamp
local timestamp = os.time()

-- Parse the received data into a key-value map
local data_map = {}
for _, kvs in ipairs(txt) do
    local kv = {}
    for match in string.gmatch(kvs, "[^=]+") do
        table.insert(kv, match)
    end
    if #kv > 1 then
        local key = string.gsub(kv[1], "%s+$", "") -- Replace trailing spaces with no space
        local value = string.gsub(kv[2], "^%s+", "") -- Remove leading spaces
        value = string.gsub(value, "%s+$", "")       -- Remove trailing spaces
        data_map[key] = tonumber(value) or value -- Convert numeric values if possible
    end
end

-- for key, value in pairs(data_map) do
--     print(key, value)
-- end

-- Insert the data into the database
local insert_query = string.format([[
INSERT INTO sensor_data (
    id, current_panel, voltage_panel, power_panel, battery_voltage,
    battery_current, load_current, charging_status, pwm, out_temp, sys_temp, 
    pv_float) VALUES (
    %s, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, '%s', %.2f, %.2f, %.2f, %.2f
);
]],
    timestamp,
    data_map["Current (panel)"] or 0,
    data_map["Voltage (panel)"] or 0,
    data_map["Power (panel)"] or 0,
    data_map["Battery Voltage"] or 0,
    data_map["Battery Current"] or 0,
    data_map["Load Current"] or 0,
    data_map["Charging"] or "unknown",
    data_map["pwm"] or 0,
    data_map["OUT Temp"] or 0,
    data_map["SYS Temp"] or 0,
    data_map["PVF"] or 0
)
-- print(insert_query)
assert(conn:execute(insert_query))
-- Close the database connection
conn:close()
env:close()

local firebase_url = config.firebase_url .. "sensors.json"

-- Prepare data for Firebase
local firebase_data = {
    id = timestamp,
    current_panel = data_map["Current (panel)"] or 0,
    voltage_panel = data_map["Voltage (panel)"] or 0,
    power_panel = data_map["Power (panel)"] or 0,
    battery_voltage = data_map["Battery Voltage"] or 0,
    battery_current = data_map["Battery Current"] or 0,
    load_current = data_map["Load Current"] or 0,
    charging_status = data_map["Charging"] or "unknown",
    pwm = data_map["pwm"] or 0,
    out_temp = data_map["OUT Temp"] or 0,
    sys_temp = data_map["SYS Temp"] or 0,
    pv_float = data_map["PVF"] or 0
}

-- Convert data to JSON
local payload = json.encode(firebase_data)

-- Send data to Firebase
local response_body = {}
local res, code, headers, status = http.request{
    url = firebase_url,
    method = "POST",
    headers = {
        ["Content-Type"] = "application/json",
        ["Content-Length"] = tostring(#payload)
    },
    source = ltn12.source.string(payload),
    sink = ltn12.sink.table(response_body)
}

-- Check Firebase response
if code == 200 then
    print("Data successfully pushed to Firebase.")
else
    print("Failed to push data to Firebase. HTTP Code:", code)
    print("Error:", table.concat(response_body))
end

print("Data has been successfully stored in the SQLite database.")
