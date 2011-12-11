
let command = Cmd.locate("testHttp") + " --filter mpr.api.http --iterations 2 " + test.mapVerbosity(-1)
Cmd.run(command)
