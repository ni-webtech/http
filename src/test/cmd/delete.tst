/*
 *  delete.tst - Test http delete
 */

require ejs.test
load("http/support.es")

//  DELETE
run("http/test.dat /tmp/test.dat")
assert(exists("web/tmp/test.dat"))
run("--method DELETE /tmp/test.dat")
assert(!exists("web/tmp/test.dat"))

