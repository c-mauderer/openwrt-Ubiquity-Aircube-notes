function handle_request(env)
    uhttpd.send("Status: 200 OK\r\n")
    uhttpd.send("Content-Type: text/html\r\n\r\n")

    local command = "/bin/sh /tmp/persistent/config/patch/patch.sh"

    local proc = assert(io.popen(command))
    for line in proc:lines() do
        uhttpd.send(line.."\n")
    end
    proc:close()
end
