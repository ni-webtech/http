/*
    auth.tst - Test authentication
 */

load("support.es")

//  Auth
//  TODO - should test failure
run("--user 'joshua:pass1' /basic/basic.html")
run("--user 'joshua' --password 'pass1' /basic/basic.html")

