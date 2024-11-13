Thing around which the world turns.

<code>
Step 0: Makefile
Step 1: HTTP server. Stupid basic.                   
Step 2: HTTPS server.                                
Step 2.5: JSON support                               <---- you are here
Step 2.6: General support for an oauth.
Step 3: WEBRTC plumbing for connectionless sockets.
Step 4: Game goes here.
</code>

On OSX...you may have to:

<code>
export CFLAGS="-I/opt/homebrew/Cellar/openssl@3/3.3.1/include -L/opt/homebrew/Cellar/openssl@3/3.3.1/lib"
</code>
