/*
    empty.tst - Empty response
 */

load("support.es")

//  Empty get
data = run("/empty.html")
assert(data == "")

