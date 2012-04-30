/*
    delete.tst - Test http delete
 */

load("support.es")

//  DELETE
run("test.dat /tmp/test.dat")
assert(Path("web/tmp/test.dat").exists)
run("--method DELETE /tmp/test.dat")
assert(!Path("web/tmp/test.dat").exists)

