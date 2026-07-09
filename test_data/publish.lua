#!/usr/bin/lua

local function send(status, meta)
    io.write(status .. " " .. meta .. "\r\n")
end

local data_length = tonumber(os.getenv("DATA_LENGTH") or "0")

if data_length == 0 then
    send("2", "text/gemini; charset=utf-8")
    io.write("# Publish\n\n")
    io.write("=: /publish.lua Post a page\n")
    return
end

local body = io.read(data_length)
if not body then
    send("4", "Could not read data")
    return
end

local filename, content = body:match("^([^\n]+)\n(.+)$")
if not filename or not content then
    send("4", "Invalid format: expected filename on first line followed by content")
    return
end

filename = filename:gsub("[^%w%-%_]", "")
if filename == "" then
    send("4", "Invalid filename")
    return
end
filename = filename .. ".gmi"

local serve_dir = arg and arg[0] and arg[0]:match("^(.*)/[^/]+$") or "."
local filepath = serve_dir .. "/pages/" .. filename

local f, err = io.open(filepath, "w")
if not f then
    send("5", "Could not write file: " .. err)
    return
end
f:write(content .. "\n")
f:close()

send("3", "/pages/" .. filename)
