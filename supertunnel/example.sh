# Start HTTP Tunnel from classes directory: java -classpath . jw.supertunnel.server....

# Sudo - Start Tunnel Server (listens on port 80) and parameter is where to route the Tunnel traffic to

# Start Tunnel Client - telling where is the Tunnel Server listening (for local tests - localhost:80) - allocates port (10229 by default) for the Tunnel data

# start sudo /usr/bin/sshd -p 2222 to build a taret servce

# test target service cnnectivity (ssh localhost -p 2222) - works fine

# now test target service via HTTP tunnel - ssh localhost -p 10229 (TunnelClient Port)- works fine - is slower than directly works in "bursts"
