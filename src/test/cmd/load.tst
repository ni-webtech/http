/*
    load.tst - Load tests
 */

load("support.es")

//  Load test
if (test.depth > 2) {
    run("-i 2000 /index.html")
    run("-i 2000 /big.txt")
}
