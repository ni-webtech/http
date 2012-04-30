/*
    basic.tst - Basic http tests
 */

load("support.es")

run("/index.html").match("Hello /index.html")
