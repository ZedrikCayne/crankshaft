#Crankshaft: The thing around which the world turns.

```
Step 0: Makefile
Step 1: HTTP server. Stupid basic.                   
Step 2: HTTPS server.                                
Step 2.5: JSON support                 
Step 2.6: General support for an oauth.              <---- you are here
Step 3: WEBRTC plumbing for connectionless sockets.
Step 4: Game goes here.
```

Building a web server from the ground up because I can. Building it up the way I think it should be done. Hopefully not making too too many mistakes along the way. The entire thing is very opinionated.

We're getting there. Finally got outgoing http connections working.

##Basics
```
scripts/gentest
make clean test
```
Will run the unit tests... (All c/c++ files in the tests folder are considered unit tests...)
The `scripts/gentest` script generates `test/autogen-test.c` which will get built into the application. The supplied application (main.cpp) is a basic webserver with a special case /api route that just returns back out what was sent in. The server supports get/post on the api and get on the basic file server. The main purpose of it is to drive the unit tests `--test`

Unit tests are pretty comprehensive, including starting the internal web server, and using the internal http request system to hit it. If you want *all* of the tests...in the Makefile there are lines to exclude the tests of the tests that you can comment out. (Note, the unit tests will *fail* because we test failing the tests... Which is why we've got them commented out...)

At the moment it'll compile clean with no warnings on osx and wsl. It'll probably compile clean on any recent linux distro provided you've got openssl-dev libraries installed. I'm pretty sure that's the only lib that isn't standard that crankshaft depends on.

```
make clean all
```
Builds without the tests. Running --test will exit with a warning that no tests were defined.

```
make clean publish
```
Builds a lib, and tarball packaged with the include directory and associated .a file.
