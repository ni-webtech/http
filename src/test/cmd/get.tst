/*
    get.tst - Test http get 
 */

load("support.es")

//  Basic get
data = run("/numbers.txt")
assert(data.startsWith("012345678"))
assert(data.endsWith("END"))
