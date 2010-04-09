/*
 *  headers.tst - Test http --showHeaders
 */

require ejs.test
load("http/support.es")

//  Validate that header appears
data = run("--showHeaders --header 'custom: MyHeader' /index.html 2>&1")
assert(data.contains('content-type'))
