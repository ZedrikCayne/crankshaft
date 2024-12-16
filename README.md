#Crankshaft: The thing around which the world turns.

```
Step 0: Makefile
Step 1: HTTP server. Stupid basic.                   
Step 2: HTTPS server.                                
Step 2.5: JSON support                 
Step 2.5.5: Storage
Step 2.6: General support for an oauth.              <---- you are here
Step 3: WEBRTC plumbing for connectionless sockets.
Step 4: Game goes here.
```

Building a web server from the ground up because I can. Building it up the way I think it should be done. Hopefully not making too too many mistakes along the way. The entire thing is very opinionated.

We're getting there. Finally got outgoing http connections working. Took a detour to get some hard storage plugged in so I can do stuff like...save state.

##Basics
```
scripts/gentest
make clean test
```
Will run the unit tests... (All c/c++ files in the tests folder are considered unit tests...)

At the moment it'll compile clean with no warnings on osx and wsl. It'll probably compile clean on any recent linux distro provided you've got openssl-dev libraries installed. I'm pretty sure that's the only lib that isn't standard that crankshaft depends on.

```
make clean all
```
Builds the app without the tests. Running --test will exit with a warning that no tests were defined.

```
make clean publish
```
Builds a lib, and tarball packaged with the include directory and associated .a file. Does not include main.cpp (Or any .cpp files) or any of the tests.

The main.cpp app has lots of switches and twiddles, check out the --help.

