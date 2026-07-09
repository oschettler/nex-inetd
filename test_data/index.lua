#!/usr/bin/lua

io.write("2 text/gemini; charset=utf-8\r\n")

io.write("# Spartan CGI Example\n\n")

io.write("## Current Time\n")
io.write(os.date("%Y-%m-%d %H:%M:%S") .. "\n\n")

io.write("## Environment\n")
local vars = { "SERVER_NAME", "SERVER_PORT", "DATA_LENGTH", "REMOTE_ADDR" }
for _, name in ipairs(vars) do
    local val = os.getenv(name) or "(not set)"
    io.write(name .. ": " .. val .. "\n")
end
