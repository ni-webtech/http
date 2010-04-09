/*
 *  testMpr.tst - Valgrind testMpr on Unix-like systems
 */

require ejs.test

if (test.config["BLD_HOST_OS"] == "LINUX") {
    let command = locate("testMpr") + " --iterations 5 "
    let valgrind = "/usr/bin/env valgrind -q --tool=memcheck --suppressions=mpr.supp " + command + test.mapVerbosity(-1)

    if (test.depth >= 2) {
        testCmdNoCapture(command)
        if (test.multithread) {
            testCmdNoCapture(valgrind + " --threads 8")
        }
    }

} else {
    test.skip("Run on Linux")
}
