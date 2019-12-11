function handle_request(env)
    uhttpd.send("Status: 200 OK\r\n")
    uhttpd.send("Content-Type: text/plain\r\n\r\n")

    local command = "/bin/sh /tmp/persistent/config/patch/patch.sh 2>&1"

    local proc = assert(io.popen(command))
    for line in proc:lines() do
        uhttpd.send(line.."\r\n")
    end
    proc:close()
end
