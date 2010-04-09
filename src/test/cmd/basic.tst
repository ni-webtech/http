/*
 *  basic.tst - Basic http tests
 */

require ejs.test
load("http/support.es")

assert(sh(env() + command + "/index.html").match("Hello /index.html"))
