
require ejs.test

let command = locate("testMpr") + " --filter mpr.api.http --iterations 2 " + test.mapVerbosity(-1)
testCmdNoCapture(command)
