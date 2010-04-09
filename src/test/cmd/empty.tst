/*
 *  empty.tst - Empty response
 */

require ejs.test
load("http/support.es")

//  Empty get
data = run("/empty.html")
assert(data == "")

