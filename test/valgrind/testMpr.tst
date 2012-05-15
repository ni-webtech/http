/*
    testHttp.tst - Valgrind testHttp on Unix-like systems
 */

if (test.config["BIT_HOST_OS"] == "LINUX") {
    let command = locate("testHttp") + " --iterations 5 "
    let valgrind = "/usr/bin/env valgrind -q --tool=memcheck --suppressions=mpr.supp " + command + test.mapVerbosity(-1)

    if (test.depth >= 2) {
        run(command)
        if (test.multithread) {
            run(valgrind + " --threads 8")
        }
    }

} else {
    test.skip("Run on Linux")
}
