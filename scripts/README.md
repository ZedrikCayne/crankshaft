Scripts in this folder:

# genmime

Creates the include files for the server for mime types. If you need/want more mime types add them to the mimedata/mimedata.csv file and run the script.

# gentest

Creates the test/autogen-test.cpp file. Without arguments it will add every file starting starting with `test-` and it assumes there is a function called `test_` inside.

For example, `test/test-test1.c` has a callable function inside `bool test_test1()` that the generated `test/autogen-test.cpp` will call.

# setGOOGLE_JSON

In bash, run `. scripts/setGOOGLE_JSON` which will find an exported google client secret you exported from your google project so you can do google logins. Sets it to an environment variable for `src/main.cpp` to consume.

In practice the value of that environment variable should probably be set by a secrets manager or through some other method. That environment variable is not hard coded into the googleservices source.

# stub

Requires an argument, the name of the file you want to add. It will helpfully stub out an implementation and header file for the server library.

`stub foo` will create `src/foo.c` and `include/crankshaft/foo.h` along with the default header guards and basic includes like `stdbool.h`

# stubtest

Requires an argument, the name of the test you want to add. Be warned, you can accidentally overwrite a test that is already there.

`stubtest binkle` will create `test/test-bilkle.c` with default headers and the skeleton of the standard test bits.
