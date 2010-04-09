/*
 *  get.tst - Test http get 
 */

require ejs.test
load("http/support.es")

//  Basic get
data = run("/numbers.txt")
assert(data.startsWith("012345678"))
assert(data.endsWith("END"))
